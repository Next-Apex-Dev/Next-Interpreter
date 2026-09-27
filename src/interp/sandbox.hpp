// 沙箱路径校验器 - 对齐 spec 7.4 / design 2.3.2.5
// 校验文件路径是否在允许的根目录内，防止越界访问
// 锁冲突抛 RUN010，I/O 失败抛 RUN011，沙箱越界抛 RUN012
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

namespace next11 {

namespace fs = std::filesystem;

class SandboxChecker {
public:
    // 默认允许当前工作目录及其子目录
    SandboxChecker() {
        _roots.push_back(fs::absolute(fs::current_path()).lexically_normal());
    }

    // 添加允许的根目录
    void add_root(const std::string& root) {
        std::lock_guard<std::mutex> lk(_mtx);
        _roots.push_back(fs::absolute(fs::path(root)).lexically_normal());
    }

    // 设置允许的根目录列表（替换）
    void set_roots(const std::vector<std::string>& roots) {
        std::lock_guard<std::mutex> lk(_mtx);
        _roots.clear();
        for (const auto& r : roots) {
            _roots.push_back(fs::absolute(fs::path(r)).lexically_normal());
        }
    }

    // 校验路径是否在任一允许的根目录内
    // 返回 true 表示允许，false 表示越界
    bool validate(const std::string& path) const {
        std::lock_guard<std::mutex> lk(_mtx);
        try {
            fs::path p = fs::absolute(fs::path(path)).lexically_normal();
            // 规范化路径（不要求存在）
            std::string ps = p.string();
            for (const auto& root : _roots) {
                std::string rs = root.string();
                // 路径以根目录为前缀则允许
                if (ps == rs || (ps.size() > rs.size() &&
                    ps.compare(0, rs.size(), rs) == 0 &&
                    (ps[rs.size()] == '/' || ps[rs.size()] == '\\'))) {
                    return true;
                }
            }
            return false;
        } catch (...) {
            return false;
        }
    }

    // 校验并返回规范化的绝对路径
    // 越界时抛 std::runtime_error("RUN012: ...")
    std::string resolve(const std::string& path) const {
        if (!validate(path)) {
            throw std::runtime_error("RUN012: 沙箱越界访问: " + path);
        }
        try {
            return fs::absolute(fs::path(path)).lexically_normal().string();
        } catch (...) {
            throw std::runtime_error("RUN011: 路径解析失败: " + path);
        }
    }

    // 校验路径是否可写（在根目录内且父目录存在）
    bool can_write(const std::string& path) const {
        if (!validate(path)) return false;
        try {
            fs::path p = fs::absolute(fs::path(path)).lexically_normal();
            fs::path parent = p.parent_path();
            return parent.empty() || fs::exists(parent);
        } catch (...) {
            return false;
        }
    }

    // 校验路径是否可读（在根目录内且文件存在）
    bool can_read(const std::string& path) const {
        if (!validate(path)) return false;
        try {
            return fs::exists(fs::path(path));
        } catch (...) {
            return false;
        }
    }

    // 获取允许的根目录列表
    std::vector<std::string> roots() const {
        std::lock_guard<std::mutex> lk(_mtx);
        std::vector<std::string> r;
        for (const auto& root : _roots) r.push_back(root.string());
        return r;
    }

private:
    mutable std::mutex _mtx;
    std::vector<fs::path> _roots;
};

} // namespace next11