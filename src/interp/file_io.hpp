// 文件 I/O 操作模块 - 对齐 spec 7.4 / design 2.3.2.5
// 提供 open/read/write/close/stat/lock/unlock/atomic_write/async_read/async_write
// 编码检测：BOM（UTF-8: EF BB BF, UTF-16 LE: FF FE），显式 encoding，默认 UTF-8
// 锁冲突抛 RUN010，I/O 失败抛 RUN011，沙箱越界抛 RUN012
#pragma once
#include "common/value.hpp"
#include "sandbox.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <random>

// Windows 文件锁 API
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace next11 {

namespace fs = std::filesystem;

// Unicode 路径 fopen 辅助函数
// Windows 上 std::fopen 不支持中文/Unicode 路径，使用 _wfopen 替代
inline FILE* fopen_unicode(const std::string& path, const std::string& mode) {
#ifdef _WIN32
    std::wstring wpath = fs::path(path).wstring();
    std::wstring wmode(mode.begin(), mode.end());
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode.c_str());
#endif
}

class FileIO {
public:
    explicit FileIO(SandboxChecker* sandbox = nullptr) : _sandbox(sandbox) {}

    // ===== open(path, mode, encoding, buffer_size) =====
    // mode: "r"/"w"/"a"/"rb"/"wb"/"ab"/"r+"/"w+"/"a+"/... (fopen 兼容)
    // encoding: "utf-8"/"utf-16"/"gbk"/"" (空表示二进制或自动检测)
    // buffer_size: 缓冲区大小，默认 8192
    Value open(const std::string& path, const std::string& mode,
               const std::string& encoding = "utf-8", int buffer_size = 8192) {
        validate_path(path);

        std::string resolved = resolve_path(path);
        FILE* fp = fopen_unicode(resolved, mode);
        if (!fp) {
            throw std::runtime_error("RUN011: 打开文件失败: " + path + " (mode=" + mode + ")");
        }

        // 设置缓冲区
        if (buffer_size > 0 && buffer_size != 8192) {
            std::setvbuf(fp, nullptr, _IOFBF, static_cast<size_t>(buffer_size));
        }

        FileHandleData h;
        h.path = path;
        h.mode = mode;
        h.encoding = encoding.empty() ? "utf-8" : encoding;
        h.buffer_size = buffer_size;
        h.is_binary = (mode.find('b') != std::string::npos);
        h.native_handle = fp;
        h.is_open = true;
        h._handle_owner = std::shared_ptr<FILE>(fp, [](FILE* f) { if (f) std::fclose(f); });
        h.lock_kind = 0;
        return Value::make_file_handle(std::move(h));
    }

    // ===== read(handle, size) =====
    // size < 0 表示读到 EOF；二进制返回 Bytes，文本返回 string
    Value read(Value& handle, int64_t size = -1) {
        if (!handle.is_file_handle()) {
            throw std::runtime_error("RUN011: read 参数不是文件句柄");
        }
        auto& h = handle.as_file_handle();
        if (!h.is_open || !h.native_handle) {
            throw std::runtime_error("RUN011: 文件句柄已关闭");
        }
        FILE* fp = static_cast<FILE*>(h.native_handle);

        if (h.is_binary) {
            // 二进制读取
            std::vector<uint8_t> buf;
            if (size < 0) {
                // 读到 EOF，限制最大 1GB 防止 OOM
                const size_t CHUNK = 4096;
                const size_t MAX_READ = 1024ull * 1024 * 1024;
                std::vector<uint8_t> tmp(CHUNK);
                size_t n;
                while ((n = std::fread(tmp.data(), 1, CHUNK, fp)) > 0) {
                    buf.insert(buf.end(), tmp.begin(), tmp.begin() + n);
                    if (buf.size() > MAX_READ) throw std::runtime_error("RUN349: read 读取数据超过 1GB 限制");
                }
            } else {
                if (size > 1024ll * 1024 * 1024) throw std::runtime_error("RUN349: read 读取数据超过 1GB 限制");
                buf.resize(static_cast<size_t>(size));
                size_t n = std::fread(buf.data(), 1, static_cast<size_t>(size), fp);
                buf.resize(n);
            }
            return Value::make_bytes(std::move(buf));
        } else {
            // 文本读取
            std::string content;
            if (size < 0) {
                // 读到 EOF，限制最大 1GB 防止 OOM
                const size_t CHUNK = 4096;
                const size_t MAX_READ = 1024ull * 1024 * 1024;
                std::vector<char> tmp(CHUNK);
                size_t n;
                while ((n = std::fread(tmp.data(), 1, CHUNK, fp)) > 0) {
                    content.append(tmp.data(), n);
                    if (content.size() > MAX_READ) throw std::runtime_error("RUN349: read 读取数据超过 1GB 限制");
                }
            } else {
                if (size > 1024ll * 1024 * 1024) throw std::runtime_error("RUN349: read 读取数据超过 1GB 限制");
                content.resize(static_cast<size_t>(size));
                size_t n = std::fread(content.data(), 1, static_cast<size_t>(size), fp);
                content.resize(n);
            }
            // BOM 检测与剥离
            content = strip_bom(content, h.encoding);
            return Value::make_string(std::move(content));
        }
    }

    // ===== write(handle, data) =====
    // data: string 或 Bytes；返回写入字节数
    Value write(Value& handle, const Value& data) {
        if (!handle.is_file_handle()) {
            throw std::runtime_error("RUN011: write 参数不是文件句柄");
        }
        auto& h = handle.as_file_handle();
        if (!h.is_open || !h.native_handle) {
            throw std::runtime_error("RUN011: 文件句柄已关闭");
        }
        FILE* fp = static_cast<FILE*>(h.native_handle);

        size_t written = 0;
        if (data.is_bytes()) {
            const auto& b = data.as_bytes().data;
            if (!b.empty()) {
                written = std::fwrite(b.data(), 1, b.size(), fp);
            }
        } else if (data.is_string()) {
            const auto& s = data.as_string();
            if (!s.empty()) {
                // 文本模式：若需要 BOM 且文件为空则写入 BOM
                if (h.encoding == "utf-8-sig" && std::ftell(fp) == 0) {
                    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
                    std::fwrite(bom, 1, 3, fp);
                }
                written = std::fwrite(s.data(), 1, s.size(), fp);
            }
        } else {
            throw std::runtime_error("RUN011: write 数据必须是 string 或 bytes");
        }
        std::fflush(fp);
        return Value::make_int(static_cast<int64_t>(written));
    }

    // ===== close(handle) =====
    Value close(Value& handle) {
        if (!handle.is_file_handle()) {
            throw std::runtime_error("RUN330: close 参数不是文件句柄");
        }
        auto& h = handle.as_file_handle();
        if (h.is_open && h.native_handle) {
            // 释放锁（如有）
            if (h.lock_kind != 0) {
                try { unlock(handle); }
                catch (const std::exception& e) {
                    std::fprintf(stderr, "警告: close 时释放锁失败: %s\n", e.what());
                }
            }
            FILE* fp = static_cast<FILE*>(h.native_handle);
            std::fclose(fp);
            h.native_handle = nullptr;
            h.is_open = false;
            h._handle_owner.reset();
            h.lock_kind = 0;
        }
        return Value::make_bool(true);
    }

    // ===== stat(path) =====
    // 返回 DictData（size/mtime/mode/type）
    Value stat(const std::string& path) {
        validate_path(path);
        std::string resolved = resolve_path(path);
        auto p = fs::path(resolved);

        if (!fs::exists(p)) {
            throw std::runtime_error("RUN011: 文件不存在: " + path);
        }

        auto dict = std::make_shared<DictData>();
        std::error_code ec;

        // size
        int64_t sz = 0;
        if (fs::is_regular_file(p, ec)) {
            sz = static_cast<int64_t>(fs::file_size(p, ec));
        }
        dict->entries.emplace_back(Value::make_string("size"), Value::make_int(sz));

        // mtime (Unix 毫秒)
        auto ftime = fs::last_write_time(p, ec);
        auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime);
        auto epoch = sctp.time_since_epoch();
        int64_t mtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        dict->entries.emplace_back(Value::make_string("mtime"), Value::make_int(mtime_ms));

        // mode (权限位，Windows 简化为 0666/0444/0755)
        int64_t mode_bits = 0644;
        if (fs::is_directory(p, ec)) mode_bits = 0755;
        else if (fs::is_regular_file(p, ec)) {
            // 检查可写
            std::ofstream test(p, std::ios::app);
            mode_bits = test.good() ? 0664 : 0444;
        }
        dict->entries.emplace_back(Value::make_string("mode"), Value::make_int(mode_bits));

        // type
        std::string type_str = "unknown";
        if (fs::is_regular_file(p, ec)) type_str = "file";
        else if (fs::is_directory(p, ec)) type_str = "dir";
        else if (fs::is_symlink(p, ec)) type_str = "symlink";
        dict->entries.emplace_back(Value::make_string("type"), Value::make_string(type_str));

        return Value::make_dict(dict);
    }

    // ===== lock(handle, kind) / unlock(handle) =====
    // kind: 1=shared (读锁), 2=exclusive (写锁)
    // 锁冲突抛 RUN010
    Value lock(Value& handle, int kind) {
        if (!handle.is_file_handle()) {
            throw std::runtime_error("RUN011: lock 参数不是文件句柄");
        }
        auto& h = handle.as_file_handle();
        if (!h.is_open || !h.native_handle) {
            throw std::runtime_error("RUN011: 文件句柄已关闭");
        }
        if (h.lock_kind != 0) {
            throw std::runtime_error("RUN010: 文件已加锁");
        }

#ifdef _WIN32
        // Windows: LockFileEx
        HANDLE hFile = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(static_cast<FILE*>(h.native_handle))));
        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("RUN011: 获取 Windows 文件句柄失败");
        }
        OVERLAPPED ov = {};
        ov.Offset = 0;
        ov.OffsetHigh = 0;
        // 锁定整个文件 (0..0x7FFFFFFF)
        BOOL ok;
        if (kind == 2) {
            // 独占锁
            ok = LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                            0, 0x7FFFFFFF, 0, &ov);
        } else {
            // 共享锁
            ok = LockFileEx(hFile, LOCKFILE_FAIL_IMMEDIATELY,
                            0, 0x7FFFFFFF, 0, &ov);
        }
        if (!ok) {
            throw std::runtime_error("RUN010: 文件锁冲突");
        }
#else
        // POSIX: flock
        int fd = fileno(static_cast<FILE*>(h.native_handle));
        int op = (kind == 2) ? (LOCK_EX | LOCK_NB) : (LOCK_SH | LOCK_NB);
        if (flock(fd, op) != 0) {
            throw std::runtime_error("RUN010: 文件锁冲突");
        }
#endif
        h.lock_kind = kind;
        return Value::make_bool(true);
    }

    Value unlock(Value& handle) {
        if (!handle.is_file_handle()) {
            throw std::runtime_error("RUN331: unlock 参数不是文件句柄");
        }
        auto& h = handle.as_file_handle();
        if (h.lock_kind == 0) return Value::make_bool(true);
        if (!h.is_open || !h.native_handle) {
            h.lock_kind = 0;
            return Value::make_bool(true);
        }

#ifdef _WIN32
        HANDLE hFile = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(static_cast<FILE*>(h.native_handle))));
        if (hFile != INVALID_HANDLE_VALUE) {
            OVERLAPPED ov = {};
            UnlockFileEx(hFile, 0, 0x7FFFFFFF, 0, &ov);
        }
#else
        int fd = fileno(static_cast<FILE*>(h.native_handle));
        flock(fd, LOCK_UN);
#endif
        h.lock_kind = 0;
        return Value::make_bool(true);
    }

    // ===== atomic_write(path, content) =====
    // 临时文件 → rename，保证原子性
    Value atomic_write(const std::string& path, const Value& content) {
        validate_path(path);
        std::string resolved = resolve_path(path);
        auto p = fs::path(resolved);

        // 生成临时文件名（同目录，确保同卷）
        std::string tmp_suffix = ".tmp.";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 999999);
        tmp_suffix += std::to_string(dist(gen));

        auto tmp_path = fs::path(resolved + tmp_suffix);

        try {
            // 写入临时文件
            FILE* fp = fopen_unicode(tmp_path.string(), "wb");
            if (!fp) {
                throw std::runtime_error("RUN011: 创建临时文件失败: " + tmp_path.string());
            }
            size_t written = 0;
            if (content.is_string()) {
                const auto& s = content.as_string();
                if (!s.empty()) {
                    written = std::fwrite(s.data(), 1, s.size(), fp);
                }
            } else if (content.is_bytes()) {
                const auto& b = content.as_bytes().data;
                if (!b.empty()) {
                    written = std::fwrite(b.data(), 1, b.size(), fp);
                }
            } else {
                std::fclose(fp);
                fs::remove(tmp_path);
                throw std::runtime_error("RUN011: atomic_write 数据必须是 string 或 bytes");
            }
            std::fflush(fp);
            std::fclose(fp);

            if (content.is_string() && written != content.as_string().size()) {
                fs::remove(tmp_path);
                throw std::runtime_error("RUN011: 临时文件写入不完整");
            }

            // 原子重命名（Windows 上 fs::rename 会覆盖目标）
            std::error_code ec;
            fs::rename(tmp_path, p, ec);
            if (ec) {
                fs::remove(tmp_path, ec);
                throw std::runtime_error("RUN011: 原子重命名失败: " + ec.message());
            }
            return Value::make_int(static_cast<int64_t>(written));
        } catch (const std::runtime_error&) {
            throw;
        } catch (const std::exception& e) {
            fs::remove(tmp_path);
            throw std::runtime_error(std::string("RUN011: atomic_write 失败: ") + e.what());
        }
    }

    // ===== async_read(path, encoding) =====
    // 简化：同步读取返回结果（无真实异步调度）
    Value async_read(const std::string& path, const std::string& encoding = "utf-8") {
        validate_path(path);
        std::string resolved = resolve_path(path);

        FILE* fp = fopen_unicode(resolved, "rb");
        if (!fp) {
            throw std::runtime_error("RUN011: async_read 打开文件失败: " + path);
        }

        // 读取全部内容，限制最大 1GB 防止 OOM
        std::vector<uint8_t> buf;
        const size_t CHUNK = 8192;
        const size_t MAX_READ = 1024ull * 1024 * 1024;
        std::vector<uint8_t> tmp(CHUNK);
        size_t n;
        while ((n = std::fread(tmp.data(), 1, CHUNK, fp)) > 0) {
            buf.insert(buf.end(), tmp.begin(), tmp.begin() + n);
            if (buf.size() > MAX_READ) throw std::runtime_error("RUN349: async_read 读取数据超过 1GB 限制");
        }
        std::fclose(fp);

        // 编码处理
        std::string enc = encoding.empty() ? "utf-8" : encoding;
        if (enc == "binary") {
            return Value::make_bytes(std::move(buf));
        }
        std::string content(buf.begin(), buf.end());
        content = strip_bom(content, enc);
        return Value::make_string(std::move(content));
    }

    // ===== async_write(path, data) =====
    // 简化：同步写入返回结果
    Value async_write(const std::string& path, const Value& data) {
        validate_path(path);
        std::string resolved = resolve_path(path);

        FILE* fp = fopen_unicode(resolved, "wb");
        if (!fp) {
            throw std::runtime_error("RUN011: async_write 打开文件失败: " + path);
        }

        size_t written = 0;
        if (data.is_string()) {
            const auto& s = data.as_string();
            if (!s.empty()) written = std::fwrite(s.data(), 1, s.size(), fp);
        } else if (data.is_bytes()) {
            const auto& b = data.as_bytes().data;
            if (!b.empty()) written = std::fwrite(b.data(), 1, b.size(), fp);
        } else {
            std::fclose(fp);
            throw std::runtime_error("RUN011: async_write 数据必须是 string 或 bytes");
        }
        std::fflush(fp);
        std::fclose(fp);
        return Value::make_int(static_cast<int64_t>(written));
    }

private:
    SandboxChecker* _sandbox;

    void validate_path(const std::string& path) const {
        if (_sandbox) {
            if (!_sandbox->validate(path)) {
                throw std::runtime_error("RUN012: 沙箱越界访问: " + path);
            }
        }
    }

    std::string resolve_path(const std::string& path) const {
        if (_sandbox) {
            return _sandbox->resolve(path);
        }
        try {
            return fs::absolute(fs::path(path)).string();
        } catch (...) {
            return path;
        }
    }

    // BOM 检测与剥离
    // UTF-8 BOM: EF BB BF
    // UTF-16 LE BOM: FF FE
    // UTF-16 BE BOM: FE FF
    static std::string strip_bom(const std::string& content, const std::string& encoding) {
        if (content.size() >= 3) {
            unsigned char b0 = static_cast<unsigned char>(content[0]);
            unsigned char b1 = static_cast<unsigned char>(content[1]);
            unsigned char b2 = static_cast<unsigned char>(content[2]);
            // UTF-8 BOM
            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
                return content.substr(3);
            }
        }
        if (content.size() >= 2) {
            unsigned char b0 = static_cast<unsigned char>(content[0]);
            unsigned char b1 = static_cast<unsigned char>(content[1]);
            // UTF-16 LE BOM
            if (b0 == 0xFF && b1 == 0xFE) {
                // 简化：UTF-16 LE 解码为 UTF-8
                std::string result;
                for (size_t i = 2; i + 1 < content.size(); i += 2) {
                    uint16_t cp = static_cast<uint8_t>(content[i]) |
                                  (static_cast<uint8_t>(content[i + 1]) << 8);
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                }
                return result;
            }
            // UTF-16 BE BOM
            if (b0 == 0xFE && b1 == 0xFF) {
                std::string result;
                for (size_t i = 2; i + 1 < content.size(); i += 2) {
                    uint16_t cp = (static_cast<uint8_t>(content[i]) << 8) |
                                  static_cast<uint8_t>(content[i + 1]);
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                }
                return result;
            }
        }
        // 无 BOM：utf-8/binary 直接返回原内容，其他编码报错（不做静默转换）
        if (encoding.empty() || encoding == "utf-8" || encoding == "binary") {
            return content;
        }
        throw std::runtime_error("RUN011: 不支持的文本编码 '" + encoding + "'，仅支持 utf-8/utf-16（带 BOM）/binary");
    }
};

} // namespace next11