#pragma once
// IDE内嵌HTTP服务器 - 单EXE启动IDE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comdlg32.lib")
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "ide_embedded.hpp"

namespace next11 {

inline std::string get_self_path() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::string(buf);
}

inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
        else out += (char)c;
    }
    return out;
}

inline std::string json_extract_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    std::string out;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '\\' && pos + 1 < json.size()) {
            char next = json[pos + 1];
            if (next == '"') { out += '"'; pos += 2; }
            else if (next == '\\') { out += '\\'; pos += 2; }
            else if (next == 'n') { out += '\n'; pos += 2; }
            else if (next == 'r') { out += '\r'; pos += 2; }
            else if (next == 't') { out += '\t'; pos += 2; }
            else if (next == '/') { out += '/'; pos += 2; }
            else if (next == 'u' && pos + 5 < json.size()) {
                unsigned int cp = 0;
                sscanf(json.c_str() + pos + 2, "%4x", &cp);
                if (cp < 0x80) { out += (char)cp; }
                else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                pos += 6;
            } else { out += next; pos += 2; }
        } else if (c == '"') {
            break;
        } else {
            out += c;
            pos++;
        }
    }
    return out;
}

inline std::string run_process(const std::string& exe, const std::string& args, int& exitCode) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};

    std::string cmd = "\"" + exe + "\" " + args;
    CreateProcessA(nullptr, (char*)cmd.c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    std::string output;
    char buf[4096];
    DWORD readn = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &readn, nullptr) && readn > 0) {
        output.append(buf, readn);
    }
    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    exitCode = (int)code;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return output;
}

struct ChildProcess {
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    HANDLE hStdinWrite = nullptr;
    HANDLE hStdoutRead = nullptr;
    bool alive = false;
};

inline ChildProcess& child() {
    static ChildProcess c;
    return c;
}

inline void kill_child() {
    auto& c = child();
    if (c.alive) {
        TerminateProcess(c.hProcess, 1);
        CloseHandle(c.hProcess);
        CloseHandle(c.hThread);
        CloseHandle(c.hStdinWrite);
        CloseHandle(c.hStdoutRead);
        c.alive = false;
    }
}

inline std::string read_child_output(int timeoutMs) {
    auto& c = child();
    std::string output;
    char buf[4096];
    DWORD readn = 0;

    DWORD totalWait = 0;
    while (totalWait < (DWORD)timeoutMs) {
        DWORD avail = 0;
        if (PeekNamedPipe(c.hStdoutRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            ReadFile(c.hStdoutRead, buf, sizeof(buf), &readn, nullptr);
            output.append(buf, readn);
        } else {
            if (!output.empty()) break;
            Sleep(50);
            totalWait += 50;
        }
    }
    return output;
}

inline std::string check_child_status(const std::string& output) {
    auto& c = child();
    DWORD exitCode = 0;
    BOOL stillRunning = GetExitCodeProcess(c.hProcess, &exitCode) && exitCode == STILL_ACTIVE;

    if (stillRunning) {
        return "{\"output\":\"" + json_escape(output) + "\",\"running\":true}";
    } else {
        std::string fullOutput = output;
        char buf[4096];
        DWORD readn = 0;
        while (ReadFile(c.hStdoutRead, buf, sizeof(buf), &readn, nullptr) && readn > 0) {
            fullOutput.append(buf, readn);
        }
        kill_child();
        return "{\"output\":\"" + json_escape(fullOutput) + "\",\"exitCode\":" + std::to_string((int)exitCode) + "}";
    }
}

inline std::string handle_api_run(const std::string& body) {
    kill_child();

    std::string code = json_extract_string(body, "code");
    if (code.empty()) return "{\"error\":\"No code provided\"}";

    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    std::string tmpFile = std::string(tmpPath) + "next11_ide_tmp.next";
    std::ofstream f(tmpFile, std::ios::binary);
    f << code;
    f.close();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0);
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    std::string exe = get_self_path();
    std::string cmd = "\"" + exe + "\" \"" + tmpFile + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.hStdInput = hStdinRead;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, (char*)cmd.c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);

    if (!ok) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdinWrite);
        return "{\"error\":\"CreateProcess failed\"}";
    }

    auto& c = child();
    c.hProcess = pi.hProcess;
    c.hThread = pi.hThread;
    c.hStdinWrite = hStdinWrite;
    c.hStdoutRead = hStdoutRead;
    c.alive = true;

    std::string output = read_child_output(500);
    return check_child_status(output);
}

inline std::string handle_api_run_input(const std::string& body) {
    auto& c = child();
    if (!c.alive) return "{\"error\":\"No running process\"}";

    std::string input = json_extract_string(body, "input");
    input += "\n";
    DWORD written = 0;
    WriteFile(c.hStdinWrite, input.c_str(), (DWORD)input.size(), &written, nullptr);

    std::string output = read_child_output(500);
    return check_child_status(output);
}

inline std::string handle_api_repl(const std::string& body) {
    std::string code = json_extract_string(body, "code");
    if (code.empty()) return "{\"error\":\"No code\"}";

    std::string wrapped = "fn main() -> void {\n" + code + "\n}\n";
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    std::string tmpFile = std::string(tmpPath) + "next11_ide_repl.next";
    std::ofstream f(tmpFile, std::ios::binary);
    f << wrapped;
    f.close();

    std::string exe = get_self_path();
    int exitCode = 0;
    std::string output = run_process(exe, "\"" + tmpFile + "\"", exitCode);
    std::remove(tmpFile.c_str());

    return "{\"stdout\":\"" + json_escape(output) + "\",\"stderr\":\"\",\"exitCode\":" + std::to_string(exitCode) + "}";
}

inline std::string handle_api_file_open() {
    OPENFILENAMEA ofn{};
    char szFile[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Next1.1 Files (*.next)\0*.next\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        std::ifstream f(szFile, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return "{\"path\":\"" + json_escape(szFile) + "\",\"content\":\"" + json_escape(ss.str()) + "\"}";
    }
    return "{\"error\":\"cancelled\"}";
}

inline std::string handle_api_file_save(const std::string& body) {
    std::string path = json_extract_string(body, "path");
    std::string content = json_extract_string(body, "content");
    if (path.empty()) {
        OPENFILENAMEA ofn{};
        char szFile[MAX_PATH] = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = "Next1.1 Files (*.next)\0*.next\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = "next";
        if (!GetSaveFileNameA(&ofn)) return "{\"error\":\"cancelled\"}";
        path = szFile;
    }
    std::ofstream f(path, std::ios::binary);
    f << content;
    f.close();
    return "{\"path\":\"" + json_escape(path) + "\",\"saved\":true}";
}

inline std::string handle_api_file_list(const std::string& body) {
    std::string dir = json_extract_string(body, "dir");
    if (dir.empty()) dir = ".";
    if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
    std::string pattern = dir + "*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return "{\"error\":\"Cannot list dir\"}";
    std::string json = "[";
    bool first = true;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (!first) json += ",";
        first = false;
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        json += "{\"name\":\"" + json_escape(fd.cFileName) + "\",\"isDir\":" + (isDir ? "true" : "false") + "}";
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    json += "]";
    return json;
}

inline std::string handle_api_file_read(const std::string& body) {
    std::string path = json_extract_string(body, "path");
    if (path.empty()) return "{\"error\":\"No path\"}";
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "{\"error\":\"Cannot open file\"}";
    std::stringstream ss;
    ss << f.rdbuf();
    return "{\"content\":\"" + json_escape(ss.str()) + "\"}";
}


class IdeServer {
public:
    static IdeServer& instance() {
        static IdeServer s;
        return s;
    }

    void start(int port = 17890) {
        if (_running.load()) return;
        _running.store(true);

        for (int p = port; p < port + 100; ++p) {
            SOCKET testSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (testSock == INVALID_SOCKET) continue;
            int opt = 1;
            setsockopt(testSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons((u_short)p);
            if (bind(testSock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
                closesocket(testSock);
                _port = p;
                break;
            }
            closesocket(testSock);
            if (p == port + 99) { _running.store(false); return; }
        }

        _thread = std::thread([this]() { run(); });
        _thread.detach();
        std::string url = "http://127.0.0.1:" + std::to_string(_port) + "/index.html";
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void stop() {
        _running.store(false);
        if (_sock != INVALID_SOCKET) { closesocket(_sock); _sock = INVALID_SOCKET; }
    }

private:
    std::thread _thread;
    std::atomic<bool> _running{false};
    int _port = 17890;
    SOCKET _sock = INVALID_SOCKET;

    IdeServer() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~IdeServer() {
        stop();
        WSACleanup();
    }

    void run() {
        _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sock == INVALID_SOCKET) return;
        int opt = 1;
        setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((u_short)_port);
        if (bind(_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(_sock); _sock = INVALID_SOCKET; return;
        }
        if (listen(_sock, 8) == SOCKET_ERROR) {
            closesocket(_sock); _sock = INVALID_SOCKET; return;
        }
        while (_running.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(_sock, &fds);
            timeval tv{0, 100000};
            int sel = select(0, &fds, nullptr, nullptr, &tv);
            if (sel <= 0) continue;
            SOCKET client = accept(_sock, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;
            handle_client(client);
        }
    }

    void handle_client(SOCKET client) {
        std::string request;
        char buf[65536];
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { closesocket(client); return; }
        request.append(buf, n);

        size_t headerEnd = request.find("\r\n\r\n");
        while (headerEnd == std::string::npos) {
            n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            request.append(buf, n);
            headerEnd = request.find("\r\n\r\n");
        }
        if (headerEnd == std::string::npos) { closesocket(client); return; }

        std::string method = "GET";
        std::string path = "/index.html";
        size_t sp = request.find(' ');
        if (sp != std::string::npos) {
            method = request.substr(0, sp);
            size_t sp2 = request.find(' ', sp + 1);
            if (sp2 != std::string::npos) path = request.substr(sp + 1, sp2 - sp - 1);
        }
        if (path == "/") path = "/index.html";

        std::string body;
        if (headerEnd != std::string::npos) {
            body = request.substr(headerEnd + 4);
        }

        if (method == "POST") {
            size_t clPos = request.find("Content-Length:");
            if (clPos == std::string::npos) clPos = request.find("content-length:");
            if (clPos != std::string::npos) {
                size_t valStart = request.find(':', clPos) + 1;
                while (valStart < request.size() && (request[valStart] == ' ' || request[valStart] == '\r')) valStart++;
                size_t valEnd = request.find('\r', valStart);
                int contentLen = std::stoi(request.substr(valStart, valEnd - valStart));
                while ((int)body.size() < contentLen) {
                    n = recv(client, buf, sizeof(buf) - 1, 0);
                    if (n <= 0) break;
                    body.append(buf, n);
                }
            }
        }

        if (method == "POST" && path.rfind("/api/", 0) == 0) {
            std::string respBody = handle_api(path, body);
            std::string header = "HTTP/1.1 200 OK\r\n";
            header += "Content-Type: application/json; charset=utf-8\r\n";
            header += "Content-Length: " + std::to_string(respBody.size()) + "\r\n";
            header += "Access-Control-Allow-Origin: *\r\n";
            header += "Connection: close\r\n\r\n";
            int totalSent = 0;
            while (totalSent < (int)header.size()) {
                int sent = send(client, header.c_str() + totalSent, (int)header.size() - totalSent, 0);
                if (sent <= 0) break;
                totalSent += sent;
            }
            totalSent = 0;
            while (totalSent < (int)respBody.size()) {
                int sent = send(client, respBody.c_str() + totalSent, (int)respBody.size() - totalSent, 0);
                if (sent <= 0) break;
                totalSent += sent;
            }
            closesocket(client);
            return;
        }

        auto& files = ide_files();
        auto it = files.find(path);
        std::string content_type = "application/octet-stream";
        if (path.size() >= 5 && path.substr(path.size()-5) == ".html") content_type = "text/html; charset=utf-8";
        else if (path.size() >= 4 && path.substr(path.size()-4) == ".css") content_type = "text/css; charset=utf-8";
        else if (path.size() >= 3 && path.substr(path.size()-3) == ".js") content_type = "application/javascript; charset=utf-8";
        std::string body2;
        int status = 200;
        if (it != files.end()) {
            body2 = it->second;
        } else {
            status = 404;
            body2 = "404 Not Found: " + path;
            content_type = "text/plain";
        }
        std::string header = "HTTP/1.1 " + std::to_string(status) + " OK\r\n";
        header += "Content-Type: " + content_type + "\r\n";
        header += "Content-Length: " + std::to_string(body2.size()) + "\r\n";
        header += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
        header += "Connection: close\r\n\r\n";
        int totalSent = 0;
        while (totalSent < (int)header.size()) {
            int sent = send(client, header.c_str() + totalSent, (int)header.size() - totalSent, 0);
            if (sent <= 0) break;
            totalSent += sent;
        }
        totalSent = 0;
        while (totalSent < (int)body2.size()) {
            int sent = send(client, body2.c_str() + totalSent, (int)body2.size() - totalSent, 0);
            if (sent <= 0) break;
            totalSent += sent;
        }
        closesocket(client);
    }

    std::string handle_api(const std::string& path, const std::string& body) {
        if (path == "/api/run") return handle_api_run(body);
        if (path == "/api/run/input") return handle_api_run_input(body);
        if (path == "/api/repl") return handle_api_repl(body);
        if (path == "/api/file/open") return handle_api_file_open();
        if (path == "/api/file/save") return handle_api_file_save(body);
        if (path == "/api/file/list") return handle_api_file_list(body);
        if (path == "/api/file/read") return handle_api_file_read(body);
        return "{\"error\":\"Unknown API: " + json_escape(path) + "\"}";
    }
};

} // namespace next11
