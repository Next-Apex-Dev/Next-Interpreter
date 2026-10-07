// Next 开放论坛 - Linux版本（零依赖，用libcurl+POSIX socket）
// 完整论坛功能：用户登录、帖子详情、评论、点赞、搜索、分页、排序、标签
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include "forum_html.h"

static const int FORUM_PORT = 9273;
static const char* GITHUB_REPO = "Next-Apex-Dev/Next-Interpreter";
static std::atomic<bool> g_running{true};
static std::string g_ide_path;

// objcopy嵌入的Next-IDE二进制数据
extern "C" char _binary_Next_IDE_exe_start[];
extern "C" char _binary_Next_IDE_exe_end[];

// 释放内嵌解释器
std::string extract_ide() {
    if (!g_ide_path.empty()) return g_ide_path;
    g_ide_path = "/tmp/NextForum_IDE";
    size_t size = _binary_Next_IDE_exe_end - _binary_Next_IDE_exe_start;
    std::ofstream f(g_ide_path, std::ios::binary);
    f.write(_binary_Next_IDE_exe_start, size);
    f.close();
    chmod(g_ide_path.c_str(), 0755);
    return g_ide_path;
}

// URL解码
std::string url_decode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int h = (s[i+1] >= 'A') ? (s[i+1] - 'A' + 10) : (s[i+1] - '0');
            int l = (s[i+2] >= 'A') ? (s[i+2] - 'A' + 10) : (s[i+2] - '0');
            r += (char)((h << 4) | l);
            i += 2;
        } else if (s[i] == '+') r += ' ';
        else r += s[i];
    }
    return r;
}

// URL编码
std::string url_encode(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += c;
        else if (c == ' ') r += '+';
        else { char buf[4]; sprintf(buf, "%%%02X", c); r += buf; }
    }
    return r;
}

std::string get_param(const std::string& query, const std::string& key) {
    std::string k = key + "=";
    size_t pos = query.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    size_t end = query.find('&', pos);
    return url_decode(query.substr(pos, end - pos));
}

// JSON转义
std::string json_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else r += c;
    }
    return r;
}

std::string json_extract(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":";
    size_t pos = json.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        std::string r;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char c = json[pos+1];
                if (c == 'n') r += '\n';
                else if (c == 't') r += '\t';
                else if (c == 'r') r += '\r';
                else if (c == '"') r += '"';
                else if (c == '\\') r += '\\';
                else if (c == '/') r += '/';
                else r += c;
                pos += 2;
            } else r += json[pos++];
        }
        return r;
    }
    std::string r;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']')
        r += json[pos++];
    return r;
}

// libcurl回调
static size_t curl_write_cb(void* data, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)data, size * nmemb);
    return size * nmemb;
}

// HTTPS请求（libcurl，支持GET/POST/PATCH/DELETE）
std::string https_request(const std::string& method, const std::string& path,
                          const std::string& body, const std::string& token) {
    std::string url = "https://api.github.com" + path;
    CURL* curl = curl_easy_init();
    if (!curl) return "{\"error\":\"curl初始化失败\"}";

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: NextForum");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    if (!token.empty()) {
        std::string auth = "Authorization: token " + token;
        headers = curl_slist_append(headers, auth.c_str());
    }
    if (method == "POST" || method == "PATCH") {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "{\"error\":\"网络请求失败\"}";
    if (httpCode != 200 && httpCode != 201)
        return "{\"error\":\"HTTP " + std::to_string(httpCode) + "\",\"detail\":\"" + json_escape(response.substr(0, 500)) + "\"}";
    return response;
}

// === GitHub API 封装 ===

std::string github_get_issues(const std::string& labels, const std::string& sort,
                              int page, int per_page, const std::string& state) {
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues?";
    if (!labels.empty()) path += "labels=" + labels + "&";
    path += "state=" + state + "&";
    path += "sort=" + (sort.empty() ? "created" : sort) + "&";
    path += "direction=desc&";
    path += "per_page=" + std::to_string(per_page < 1 ? 20 : per_page) + "&";
    path += "page=" + std::to_string(page < 1 ? 1 : page);
    std::string result = https_request("GET", path, "", "");
    if (result.empty()) result = "[]";
    return result;
}

std::string github_search_issues(const std::string& q, const std::string& label, int page) {
    std::string query = url_encode(q) + "+repo:" + std::string(GITHUB_REPO) + "+state:open+is:issue";
    if (!label.empty()) query += "+label:" + label;
    std::string path = "/search/issues?q=" + query + "&sort=created&order=desc&per_page=20&page=" + std::to_string(page < 1 ? 1 : page);
    return https_request("GET", path, "", "");
}

std::string github_get_issue(int number) {
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues/" + std::to_string(number);
    return https_request("GET", path, "", "");
}

std::string github_get_comments(int number) {
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues/" + std::to_string(number) + "/comments?per_page=100";
    return https_request("GET", path, "", "");
}

std::string github_create_comment(const std::string& token, int number, const std::string& body) {
    std::string json_body = "{\"body\":\"" + json_escape(body) + "\"}";
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues/" + std::to_string(number) + "/comments";
    return https_request("POST", path, json_body, token);
}

std::string github_add_reaction(const std::string& token, int number, const std::string& content) {
    std::string json_body = "{\"content\":\"" + json_escape(content) + "\"}";
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues/" + std::to_string(number) + "/reactions";
    return https_request("POST", path, json_body, token);
}

std::string github_get_user(const std::string& token) {
    return https_request("GET", "/user", "", token);
}

std::string github_get_labels() {
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/labels?per_page=50";
    return https_request("GET", path, "", "");
}

std::string github_get_stats() {
    std::string path = "/repos/" + std::string(GITHUB_REPO);
    std::string repo_info = https_request("GET", path, "", "");
    std::string stats = "{";
    stats += "\"repo\":" + repo_info + ",";
    std::string d_path = "/search/issues?q=repo:" + std::string(GITHUB_REPO) + "+state:open+label:discussion&per_page=1";
    std::string d_count = json_extract(https_request("GET", d_path, "", ""), "total_count");
    stats += "\"discussion_count\":" + (d_count.empty() ? "0" : d_count) + ",";
    std::string b_path = "/search/issues?q=repo:" + std::string(GITHUB_REPO) + "+state:open+label:bug&per_page=1";
    std::string b_count = json_extract(https_request("GET", b_path, "", ""), "total_count");
    stats += "\"bug_count\":" + (b_count.empty() ? "0" : b_count) + ",";
    std::string l_path = "/search/issues?q=repo:" + std::string(GITHUB_REPO) + "+state:open+label:library&per_page=1";
    std::string l_count = json_extract(https_request("GET", l_path, "", ""), "total_count");
    stats += "\"library_count\":" + (l_count.empty() ? "0" : l_count);
    stats += "}";
    return stats;
}

std::string github_create_issue(const std::string& token, const std::string& title,
                                const std::string& body, const std::string& labels) {
    std::string labels_json = "[";
    std::istringstream ls(labels);
    std::string label;
    bool first = true;
    while (std::getline(ls, label, ',')) {
        if (!first) labels_json += ",";
        labels_json += "\"" + json_escape(label) + "\"";
        first = false;
    }
    labels_json += "]";
    std::string json_body = "{\"title\":\"" + json_escape(title) + "\",\"body\":\"" + json_escape(body) +
                            "\",\"labels\":" + labels_json + "}";
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues";
    return https_request("POST", path, json_body, token);
}

std::string github_edit_issue(const std::string& token, int number,
                              const std::string& title, const std::string& body) {
    std::string json_body = "{\"title\":\"" + json_escape(title) + "\",\"body\":\"" + json_escape(body) + "\"}";
    std::string path = "/repos/" + std::string(GITHUB_REPO) + "/issues/" + std::to_string(number);
    return https_request("PATCH", path, json_body, token);
}

// 执行Next代码
std::string run_next_code(const std::string& code) {
    char nextFile[] = "/tmp/nextforum_code_XXXXXX";
    int fd = mkstemp(nextFile);
    if (fd < 0) return "{\"error\":\"无法创建临时文件\"}";
    write(fd, code.c_str(), code.size());
    close(fd);

    std::string idePath = extract_ide();
    std::string outputFile = std::string(nextFile) + ".out";

    pid_t pid = fork();
    if (pid == 0) {
        freopen(outputFile.c_str(), "w", stdout);
        freopen(outputFile.c_str(), "a", stderr);
        execl(idePath.c_str(), "Next-IDE", "run", nextFile, nullptr);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);

    std::ifstream outf(outputFile);
    std::string output((std::istreambuf_iterator<char>(outf)), std::istreambuf_iterator<char>());
    outf.close();
    unlink(nextFile);
    unlink(outputFile.c_str());

    return "{\"output\":\"" + json_escape(output) + "\"}";
}

// HTTP响应
void http_send(int sock, int status, const std::string& content_type,
               const std::string& body) {
    std::string status_text = (status == 200) ? "OK" :
                              (status == 404) ? "Not Found" : "Internal Server Error";
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << " " << status_text << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST, PATCH, DELETE, OPTIONS\r\n"
         << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
         << "Connection: close\r\n\r\n" << body;
    std::string r = resp.str();
    send(sock, r.c_str(), r.size(), 0);
}

// 从JSON body中提取labels数组
std::string extract_labels_from_body(const std::string& body) {
    std::string labels;
    size_t lp = body.find("\"labels\":");
    if (lp != std::string::npos) {
        size_t lb = body.find('[', lp), le = body.find(']', lb);
        if (lb != std::string::npos && le != std::string::npos) {
            std::string arr = body.substr(lb + 1, le - lb - 1);
            std::istringstream as(arr);
            std::string item;
            while (std::getline(as, item, ',')) {
                size_t s = item.find('"'), e = item.rfind('"');
                if (s != std::string::npos && e > s) {
                    if (!labels.empty()) labels += ",";
                    labels += item.substr(s + 1, e - s - 1);
                }
            }
        }
    }
    return labels;
}

// 处理HTTP请求
void handle_request(int sock, const std::string& request) {
    size_t pos = request.find("\r\n");
    std::string first_line = request.substr(0, pos);
    std::string method, path;
    std::istringstream fl(first_line);
    fl >> method >> path;

    std::string body;
    size_t body_pos = request.find("\r\n\r\n");
    if (body_pos != std::string::npos) body = request.substr(body_pos + 4);

    if (method == "OPTIONS") { http_send(sock, 200, "text/plain", ""); return; }
    if (path == "/" || path == "/index.html") {
        http_send(sock, 200, "text/html; charset=utf-8", FORUM_HTML); return;
    }

    // Issues列表
    if (path.find("/api/issues") == 0 && method == "GET") {
        size_t q = path.find('?');
        std::string query = (q != std::string::npos) ? path.substr(q + 1) : "";
        std::string labels = get_param(query, "labels");
        std::string sort = get_param(query, "sort");
        std::string state = get_param(query, "state");
        std::string page_str = get_param(query, "page");
        std::string per_page_str = get_param(query, "per_page");
        if (labels.empty()) labels = "discussion";
        if (state.empty()) state = "open";
        int page = page_str.empty() ? 1 : atoi(page_str.c_str());
        int per_page = per_page_str.empty() ? 20 : atoi(per_page_str.c_str());
        http_send(sock, 200, "application/json; charset=utf-8", github_get_issues(labels, sort, page, per_page, state));
        return;
    }

    // 搜索
    if (path.find("/api/search") == 0 && method == "GET") {
        size_t q = path.find('?');
        std::string query = (q != std::string::npos) ? path.substr(q + 1) : "";
        std::string q_val = get_param(query, "q");
        std::string label = get_param(query, "label");
        std::string page_str = get_param(query, "page");
        int page = page_str.empty() ? 1 : atoi(page_str.c_str());
        if (q_val.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少搜索关键词\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_search_issues(q_val, label, page));
        return;
    }

    // 创建Issue
    if (path == "/api/issues" && method == "POST") {
        std::string token = json_extract(body, "token");
        std::string title = json_extract(body, "title");
        std::string post_body = json_extract(body, "body");
        std::string labels = extract_labels_from_body(body);
        if (labels.empty()) labels = "discussion";
        if (token.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少 GitHub Token\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_create_issue(token, title, post_body, labels));
        return;
    }

    // 编辑Issue
    if (path == "/api/issues/edit" && method == "POST") {
        std::string token = json_extract(body, "token");
        std::string number_str = json_extract(body, "number");
        std::string title = json_extract(body, "title");
        std::string post_body = json_extract(body, "body");
        if (token.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少 Token\"}"); return; }
        int number = atoi(number_str.c_str());
        if (number <= 0) { http_send(sock, 200, "application/json", "{\"error\":\"无效的帖子编号\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_edit_issue(token, number, title, post_body));
        return;
    }

    // 获取单个Issue
    if (path.find("/api/issue") == 0 && method == "GET") {
        size_t q = path.find('?');
        std::string query = (q != std::string::npos) ? path.substr(q + 1) : "";
        int number = atoi(get_param(query, "number").c_str());
        if (number <= 0) { http_send(sock, 200, "application/json", "{\"error\":\"无效的帖子编号\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_get_issue(number));
        return;
    }

    // 获取评论
    if (path.find("/api/comments") == 0 && method == "GET") {
        size_t q = path.find('?');
        std::string query = (q != std::string::npos) ? path.substr(q + 1) : "";
        int number = atoi(get_param(query, "number").c_str());
        if (number <= 0) { http_send(sock, 200, "application/json", "[]"); return; }
        std::string result = github_get_comments(number);
        if (result.empty()) result = "[]";
        http_send(sock, 200, "application/json; charset=utf-8", result);
        return;
    }

    // 发表评论
    if (path == "/api/comment" && method == "POST") {
        std::string token = json_extract(body, "token");
        std::string number_str = json_extract(body, "number");
        std::string comment_body = json_extract(body, "body");
        if (token.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少 Token\"}"); return; }
        int number = atoi(number_str.c_str());
        if (number <= 0) { http_send(sock, 200, "application/json", "{\"error\":\"无效的帖子编号\"}"); return; }
        if (comment_body.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"评论内容不能为空\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_create_comment(token, number, comment_body));
        return;
    }

    // 添加反应
    if (path == "/api/reaction" && method == "POST") {
        std::string token = json_extract(body, "token");
        std::string number_str = json_extract(body, "number");
        std::string content = json_extract(body, "content");
        if (token.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少 Token\"}"); return; }
        if (content.empty()) content = "+1";
        int number = atoi(number_str.c_str());
        if (number <= 0) { http_send(sock, 200, "application/json", "{\"error\":\"无效的帖子编号\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_add_reaction(token, number, content));
        return;
    }

    // 获取用户信息
    if (path.find("/api/user") == 0 && method == "GET") {
        size_t q = path.find('?');
        std::string query = (q != std::string::npos) ? path.substr(q + 1) : "";
        std::string token = get_param(query, "token");
        if (token.empty()) { http_send(sock, 200, "application/json", "{\"error\":\"缺少 Token\"}"); return; }
        http_send(sock, 200, "application/json; charset=utf-8", github_get_user(token));
        return;
    }

    // 获取标签
    if (path == "/api/labels" && method == "GET") {
        std::string result = github_get_labels();
        if (result.empty()) result = "[]";
        http_send(sock, 200, "application/json; charset=utf-8", result);
        return;
    }

    // 获取统计
    if (path == "/api/stats" && method == "GET") {
        http_send(sock, 200, "application/json; charset=utf-8", github_get_stats());
        return;
    }

    // 执行Next代码
    if (path == "/api/run" && method == "POST") {
        http_send(sock, 200, "application/json; charset=utf-8", run_next_code(json_extract(body, "code")));
        return;
    }

    http_send(sock, 404, "text/plain", "404 Not Found");
}

// HTTP服务器线程
void* http_server_thread(void*) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return nullptr;

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(FORUM_PORT);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) { close(server); return nullptr; }
    if (listen(server, 10) < 0) { close(server); return nullptr; }

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        timeval tv = {1, 0};
        int ret = select(server + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        int client = accept(server, nullptr, nullptr);
        if (client < 0) continue;

        pthread_t tid;
        int* pclient = new int(client);
        pthread_create(&tid, nullptr, [](void* arg) -> void* {
            int client = *(int*)arg;
            delete (int*)arg;
            char buf[8192];
            std::string request;
            struct timeval tv = {10, 0};
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            while (true) {
                int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                request.append(buf, n);
                if (request.find("\r\n\r\n") != std::string::npos) {
                    size_t cl_pos = request.find("Content-Length:");
                    if (cl_pos != std::string::npos) {
                        size_t v_start = cl_pos + 15;
                        while (v_start < request.size() && request[v_start] == ' ') v_start++;
                        int cl = atoi(request.c_str() + v_start);
                        size_t body_start = request.find("\r\n\r\n") + 4;
                        if ((int)(request.size() - body_start) >= cl) break;
                    } else break;
                }
            }
            if (!request.empty()) {
                try { handle_request(client, request); }
                catch (...) { http_send(client, 500, "text/plain", "Internal Server Error"); }
            }
            close(client);
            return nullptr;
        }, pclient);
        pthread_detach(tid);
    }
    close(server);
    return nullptr;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    pthread_t server_tid;
    pthread_create(&server_tid, nullptr, http_server_thread, nullptr);

    usleep(500000);

    std::string url = "http://127.0.0.1:" + std::to_string(FORUM_PORT);
    pid_t pid = fork();
    if (pid == 0) { execlp("xdg-open", "xdg-open", url.c_str(), nullptr); _exit(0); }

    printf("Next 开放论坛服务正在运行...\n");
    printf("浏览器已自动打开: %s\n", url.c_str());
    printf("按 Enter 键停止服务并退出...\n");
    getchar();

    g_running = false;
    usleep(200000);
    curl_global_cleanup();
    unlink(g_ide_path.c_str());
    return 0;
}
