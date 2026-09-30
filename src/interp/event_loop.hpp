// 事件循环：调度异步任务、I/O 完成事件与定时器
#pragma once
#include <coroutine>
#include <functional>
#include <vector>
#include <queue>
#include <chrono>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <thread>
#include <utility>

namespace next11 {

// 定时器回调类型
using TimerCallback = std::function<void()>;

// 定时器条目
struct TimerEntry {
    int64_t expire_time_ms = 0;  // 到期时间（毫秒时间戳）
    TimerCallback callback;
    bool cancelled = false;
};

// I/O 待处理条目
struct IoPendingEntry {
    int fd = -1;
    int op_type = 0;                 // 0=read, 1=write
    std::coroutine_handle<> handle;  // 等待的协程
    int64_t offset = 0;
    size_t size = 0;
};

class EventLoop {
public:
    EventLoop() = default;
    ~EventLoop() = default;

    // 禁止拷贝
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // 运行直到所有任务完成或被 stop() 停止
    void run_until_complete() {
        while (!_stopped && (!_ready_queue.empty() || !_timers.empty() || !_io_pending.empty())) {
            process_ready_queue();
            process_timers();
            process_io_pending();
            // 如果只有 I/O 等待且无就绪任务，避免忙等待
            if (!_stopped && _ready_queue.empty() && !_io_pending.empty()) {
                std::this_thread::yield();
            }
        }
    }

    // 停止事件循环
    void stop() { _stopped = true; }

    // 处理一轮待处理事件（用于 await 点的协作式调度）
    void poll() {
        process_ready_queue();
        process_timers();
        process_io_pending();
    }

    // 调度协程到就绪队列
    void schedule_coroutine(std::coroutine_handle<> h) {
        if (h && !h.done()) {
            _ready_queue.push(h);
        }
    }

    // 提交异步读请求
    void submit_read(int fd, int64_t offset, size_t size, std::coroutine_handle<> h) {
        _io_pending.push_back(IoPendingEntry{fd, 0, h, offset, size});
    }

    // 提交异步写请求
    void submit_write(int fd, const std::vector<uint8_t>& data, std::coroutine_handle<> h) {
        _io_pending.push_back(IoPendingEntry{fd, 1, h, 0, data.size()});
    }

    // 添加定时器
    void add_timer(int64_t milliseconds, TimerCallback cb) {
        auto now = current_time_ms();
        _timers.push_back(TimerEntry{now + milliseconds, std::move(cb), false});
    }

    // 就绪队列大小（用于测试）
    size_t ready_queue_size() const { return _ready_queue.size(); }
    size_t timer_count() const { return _timers.size(); }
    size_t io_pending_count() const { return _io_pending.size(); }

private:
    std::queue<std::coroutine_handle<>> _ready_queue;
    std::vector<TimerEntry> _timers;
    std::vector<IoPendingEntry> _io_pending;
    std::atomic<bool> _stopped{false};

    void process_ready_queue() {
        while (!_ready_queue.empty()) {
            auto h = _ready_queue.front();
            _ready_queue.pop();
            if (h && !h.done()) h.resume();
        }
    }

    void process_timers() {
        auto now = current_time_ms();
        std::vector<TimerEntry> pending;
        pending.swap(_timers);
        for (auto& t : pending) {
            if (!t.cancelled && t.expire_time_ms <= now && t.callback) {
                t.callback();
                t.cancelled = true;
            }
        }
        for (auto& t : pending) {
            if (!t.cancelled) _timers.push_back(std::move(t));
        }
    }

    void process_io_pending() {
        std::vector<IoPendingEntry> pending;
        pending.swap(_io_pending);
        for (auto& io : pending) {
            if (io.handle && !io.handle.done()) {
                io.handle.resume();
            }
        }
    }

    static int64_t current_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

// ===== 异步运行时：管理后台 I/O 任务 =====
#include <thread>
#include <unordered_map>
#include <atomic>

class AsyncRuntime {
    struct TaskEntry {
        std::mutex mtx;
        std::shared_ptr<Value> result;
        std::string error_msg;
        bool done = false;
    };

public:
    static AsyncRuntime& instance() {
        static AsyncRuntime inst;
        return inst;
    }

    ~AsyncRuntime() {
        wait_all();
    }

    Value submit(std::function<std::shared_ptr<Value>()> task) {
        auto coro = std::make_shared<CoroutineData>();
        coro->completed = false;
        coro->result = nullptr;

        auto entry = std::make_shared<TaskEntry>();
        {
            std::lock_guard<std::mutex> lk(_tasks_mtx);
            _tasks[coro.get()] = entry;
        }
        _active_count.fetch_add(1, std::memory_order_relaxed);

        std::thread([this, entry, coro, task = std::move(task)]() {
            try {
                auto result = task();
                std::lock_guard<std::mutex> lk(entry->mtx);
                entry->result = result;
                entry->done = true;
                coro->completed = true;
                coro->result = result;
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lk(entry->mtx);
                entry->error_msg = e.what();
                entry->done = true;
                coro->completed = true;
            }
            _active_count.fetch_sub(1, std::memory_order_relaxed);
        }).detach();

        return Value::make_coroutine(coro);
    }

    bool check_done(CoroutineData* cd) {
        std::shared_ptr<TaskEntry> entry;
        {
            std::lock_guard<std::mutex> lk(_tasks_mtx);
            auto it = _tasks.find(cd);
            if (it != _tasks.end()) entry = it->second;
        }
        if (!entry) return cd->completed;

        std::string err;
        std::shared_ptr<Value> res;
        {
            std::lock_guard<std::mutex> tlk(entry->mtx);
            if (!entry->done) return false;
            err = entry->error_msg;
            res = entry->result;
        }
        cd->completed = true;
        cd->result = res;
        {
            std::lock_guard<std::mutex> tlk(_tasks_mtx);
            _tasks.erase(cd);
        }
        if (!err.empty()) {
            throw std::runtime_error(err);
        }
        return true;
    }

    void wait_all() {
        while (_active_count.load(std::memory_order_acquire) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    size_t pending_count() {
        return _active_count.load(std::memory_order_acquire);
    }

private:
    std::mutex _tasks_mtx;
    std::unordered_map<CoroutineData*, std::shared_ptr<TaskEntry>> _tasks;
    std::atomic<size_t> _active_count{0};
};

} // namespace next11