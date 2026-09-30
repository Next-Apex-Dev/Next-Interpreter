// C++20 协程运行时基础设施
// 提供生成器 (Generator) 与异步 (Coroutine) 两种协程抽象，
// 以及 I/O / 定时器 Awaitable，供 EventLoop 调度。
#pragma once
#include <coroutine>
#include <optional>
#include <variant>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <iostream>
#include <utility>

namespace next11 {

// ===== 前向声明 =====
template<typename T>
class Generator;
template<typename T>
class Coroutine;

// ===== 生成器 Promise =====
// 用于生成器函数（含 yield 的函数）
template<typename T>
struct GenPromise {
    T current_value{};              // 当前产出值
    T sent_value{};                 // send 注入的值
    std::exception_ptr thrown_exc;  // throw 注入的异常
    bool done = false;              // 是否完成

    // 返回对象必须是 Generator<T>，由协程框架在协程返回时构造
    Generator<T> get_return_object();
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T v) {
        current_value = std::move(v);
        return {};
    }

    void return_value(T v) {
        current_value = std::move(v);
        done = true;
    }

    void unhandled_exception() {
        thrown_exc = std::current_exception();
    }
};

// ===== 异步 Promise =====
// 用于 async/await 异步操作
template<typename T>
struct AsyncPromise {
    T result{};
    std::exception_ptr error;
    bool completed = false;

    // 返回对象必须是 Coroutine<T>
    Coroutine<T> get_return_object();
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_value(T v) {
        result = std::move(v);
        completed = true;
    }

    void unhandled_exception() {
        error = std::current_exception();
    }

    // await_transform 支持 IoAwaitable / TimerAwaitable 等自定义等待体
    template<typename Awaitable>
    auto await_transform(Awaitable&& a) {
        return std::forward<Awaitable>(a);
    }
};

// ===== Generator RAII 包装 =====
// RAII 包装 coroutine_handle，析构调用 destroy
template<typename T>
class Generator {
public:
    using promise_type = GenPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    Generator() = default;
    explicit Generator(handle_type h) : handle_(h) {}
    Generator(Generator&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    ~Generator() { if (handle_) handle_.destroy(); }

    // 禁止拷贝
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    bool done() const { return !handle_ || handle_.done(); }
    void resume() {
        if (handle_ && !handle_.done()) handle_.resume();
        if (handle_ && handle_.promise().thrown_exc) {
            std::rethrow_exception(handle_.promise().thrown_exc);
        }
    }
    T& value() {
        if (!handle_) throw std::runtime_error("RUN302: 生成器未初始化，不能获取值");
        if (handle_.promise().thrown_exc) std::rethrow_exception(handle_.promise().thrown_exc);
        return handle_.promise().current_value;
    }
    const T& value() const {
        if (!handle_) throw std::runtime_error("RUN302: 生成器未初始化，不能获取值");
        if (handle_.promise().thrown_exc) std::rethrow_exception(handle_.promise().thrown_exc);
        return handle_.promise().current_value;
    }
    void send(T v) {
        if (!handle_) throw std::runtime_error("RUN303: 生成器未初始化，不能发送值");
        handle_.promise().sent_value = std::move(v);
    }
    handle_type handle() const { return handle_; }

private:
    handle_type handle_ = nullptr;
};

// GenPromise::get_return_object 延迟实现（依赖 Generator 完整定义）
template<typename T>
inline Generator<T> GenPromise<T>::get_return_object() {
    return Generator<T>{std::coroutine_handle<GenPromise<T>>::from_promise(*this)};
}

// ===== Coroutine RAII 包装 =====
template<typename T>
class Coroutine {
public:
    using promise_type = AsyncPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    Coroutine() = default;
    explicit Coroutine(handle_type h) : handle_(h) {}
    Coroutine(Coroutine&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Coroutine& operator=(Coroutine&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    ~Coroutine() { if (handle_) handle_.destroy(); }

    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    bool done() const { return !handle_ || handle_.done(); }
    void resume() {
        if (handle_ && !handle_.done()) handle_.resume();
        if (handle_ && handle_.promise().error) {
            std::rethrow_exception(handle_.promise().error);
        }
    }
    T& result() {
        if (!handle_) throw std::runtime_error("RUN304: 协程未初始化，不能获取结果");
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
        return handle_.promise().result;
    }
    const T& result() const {
        if (!handle_) throw std::runtime_error("RUN304: 协程未初始化，不能获取结果");
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
        return handle_.promise().result;
    }
    bool completed() const { return handle_ && handle_.promise().completed; }
    handle_type handle() const { return handle_; }

private:
    handle_type handle_ = nullptr;
};

// AsyncPromise::get_return_object 延迟实现（依赖 Coroutine 完整定义）
template<typename T>
inline Coroutine<T> AsyncPromise<T>::get_return_object() {
    return Coroutine<T>{std::coroutine_handle<AsyncPromise<T>>::from_promise(*this)};
}

// ===== IoAwaitable =====
// 异步 I/O 等待体
struct IoAwaitable {
    int fd = -1;                       // 文件描述符
    int64_t offset = 0;                // 偏移量
    size_t size = 0;                   // 读取/写入大小
    int op_type = 0;                   // 0=read, 1=write
    std::vector<uint8_t> data;         // 写入数据或读取结果

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const noexcept {
        // 挂起协程，由 EventLoop 在 I/O 完成后恢复
        // 实际实现由 EventLoop 处理
        (void)h;
    }
    std::vector<uint8_t> await_resume() const {
        return data;
    }
};

// ===== TimerAwaitable =====
// 定时器等待体
struct TimerAwaitable {
    int64_t milliseconds = 0;

    bool await_ready() const noexcept { return milliseconds <= 0; }
    void await_suspend(std::coroutine_handle<> h) const noexcept {
        // 由 EventLoop 在定时器到期后恢复
        (void)h;
    }
    void await_resume() const noexcept {}
};

} // namespace next11