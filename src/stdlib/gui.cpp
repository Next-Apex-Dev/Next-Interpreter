// GUI 跨平台实现 - Win32 / X11 / Cocoa
#include "gui.hpp"
#include "common/value.hpp"
#include "common/registry.hpp"
#include <cstring>
#include <cctype>
#include <algorithm>

namespace next11 {

static std::string s_pending_callback;
static WidgetId s_pending_widget_id = 0;

GuiSystem& GuiSystem::instance() {
    static GuiSystem inst;
    return inst;
}

bool GuiSystem::is_valid(WidgetId id) {
    return _handles.count(id) > 0;
}

bool GuiSystem::has_callback(WidgetId widget) {
    return _click_callbacks.count(widget) > 0;
}

GuiCallback GuiSystem::get_callback(WidgetId widget) {
    auto it = _click_callbacks.find(widget);
    if (it != _click_callbacks.end()) return it->second;
    return {"", 0};
}

bool GuiSystem::has_change_callback(WidgetId widget) {
    return _change_callbacks.count(widget) > 0;
}

GuiCallback GuiSystem::get_change_callback(WidgetId widget) {
    auto it = _change_callbacks.find(widget);
    if (it != _change_callbacks.end()) return it->second;
    return {"", 0};
}

void GuiSystem::on_click(WidgetId widget, const std::string& fn_name) {
    _click_callbacks[widget] = {fn_name, widget};
}

void GuiSystem::on_change(WidgetId widget, const std::string& fn_name) {
    _change_callbacks[widget] = {fn_name, widget};
}

// ===== 颜色解析 =====
static uint32_t parse_color(const std::string& color) {
    if (color.empty()) return 0x000000;
    if (color == "red") return 0xFF0000;
    if (color == "green") return 0x00FF00;
    if (color == "blue") return 0x0000FF;
    if (color == "white") return 0xFFFFFF;
    if (color == "black") return 0x000000;
    if (color == "yellow") return 0xFFFF00;
    if (color == "cyan") return 0x00FFFF;
    if (color == "magenta") return 0xFF00FF;
    if (color == "gray" || color == "grey") return 0x808080;
    if (color == "orange") return 0xFFA500;
    std::string c = color;
    if (!c.empty() && c[0] == '#') c = c.substr(1);
    if (c.size() == 6) {
        for (char ch : c) {
            if (!std::isxdigit((unsigned char)ch)) return 0x000000;
        }
        uint32_t r = std::stoul(c.substr(0,2), nullptr, 16);
        uint32_t g = std::stoul(c.substr(2,2), nullptr, 16);
        uint32_t b = std::stoul(c.substr(4,2), nullptr, 16);
        return (r << 16) | (g << 8) | b;
    }
    return 0x000000;
}

void* GuiSystem::get_handle(WidgetId id) {
    auto it = _handles.find(id);
    if (it != _handles.end()) return it->second;
    return nullptr;
}

WidgetId GuiSystem::get_id(void* handle) {
    auto it = _handle_to_id.find(handle);
    if (it != _handle_to_id.end()) return it->second;
    return 0;
}

// =====================================================================
// Windows Win32 API 实现
// =====================================================================
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>





static LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static bool s_class_registered = false;
static const wchar_t* s_main_class = L"Next11GuiMain";
static const wchar_t* s_canvas_class = L"Next11GuiCanvas";

static void register_classes() {
    if (s_class_registered) return;
    s_class_registered = true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = GuiWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = s_main_class;
    RegisterClassExW(&wc);

    WNDCLASSEXW wc2 = {};
    wc2.cbSize = sizeof(wc2);
    wc2.lpfnWndProc = GuiWndProc;
    wc2.hInstance = GetModuleHandleW(nullptr);
    wc2.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc2.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc2.lpszClassName = s_canvas_class;
    RegisterClassExW(&wc2);
}

static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    ws.resize(len - 1);
    return ws;
}

static std::string ws2s(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr);
    s.resize(len - 1);
    return s;
}

// 画布绘图命令缓存
struct DrawCmd {
    int type; // 0=line 1=rect 2=circle 3=text 4=fillrect 5=fillcircle 6=image 7=clear
    int x1, y1, x2, y2;
    std::string text;
    std::string color;
    std::string path;
};
static std::map<HWND, std::vector<DrawCmd>> s_canvas_cmds;
static std::map<HWND, COLORREF> s_widget_bg_colors;
static std::map<HWND, HBRUSH> s_widget_brushes;
static std::map<HWND, HBITMAP> s_widget_bitmaps;
static std::map<HWND, HMENU> s_window_menus;

static LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& gui = GuiSystem::instance();

    switch (msg) {
    case WM_COMMAND: {
        HWND ctrl = (HWND)lp;
        WORD notify = HIWORD(wp);
        if (ctrl) {
            auto it = gui._handle_to_id.find(ctrl);
            if (it != gui._handle_to_id.end()) {
                WidgetId wid = it->second;
                if (notify == EN_CHANGE && gui.has_change_callback(wid)) {
                    auto cb = gui.get_change_callback(wid);
                    s_pending_callback = cb.fn_name;
                    s_pending_widget_id = wid;
                } else if (notify == BN_CLICKED && gui.has_callback(wid)) {
                    auto cb = gui.get_callback(wid);
                    s_pending_callback = cb.fn_name;
                    s_pending_widget_id = wid;
                }
            }
        }
        break;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HWND ctrl = (HWND)lp;
        auto it = s_widget_bg_colors.find(ctrl);
        if (it != s_widget_bg_colors.end()) {
            HDC hdc = (HDC)wp;
            SetBkColor(hdc, it->second);
            auto bit = s_widget_brushes.find(ctrl);
            if (bit != s_widget_brushes.end()) {
                return (LRESULT)bit->second;
            }
        }
        break;
    }
    case WM_PAINT: {
        auto it = s_canvas_cmds.find(hwnd);
        if (it != s_canvas_cmds.end()) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            for (auto& cmd : it->second) {
                uint32_t col = parse_color(cmd.color);
                HPEN pen = CreatePen(PS_SOLID, 1, RGB((col>>16)&0xFF, (col>>8)&0xFF, col&0xFF));
                HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
                HGDIOBJ oldp = SelectObject(hdc, pen);
                HGDIOBJ oldb = SelectObject(hdc, brush);
                switch (cmd.type) {
                case 0: // line
                    MoveToEx(hdc, cmd.x1, cmd.y1, nullptr);
                    LineTo(hdc, cmd.x2, cmd.y2);
                    break;
                case 1: // rect
                    Rectangle(hdc, cmd.x1, cmd.y1, cmd.x2, cmd.y2);
                    break;
                case 2: // circle
                    Ellipse(hdc, cmd.x1 - cmd.x2, cmd.y1 - cmd.x2, cmd.x1 + cmd.x2, cmd.y1 + cmd.x2);
                    break;
                case 3: { // text
                    SetTextColor(hdc, RGB((col>>16)&0xFF, (col>>8)&0xFF, col&0xFF));
                    SetBkMode(hdc, TRANSPARENT);
                    auto ws = s2ws(cmd.text);
                    TextOutW(hdc, cmd.x1, cmd.y1, ws.c_str(), (int)ws.size());
                    break;
                }
                case 4: { // fillrect
                    HBRUSH fb = CreateSolidBrush(RGB((col>>16)&0xFF, (col>>8)&0xFF, col&0xFF));
                    HGDIOBJ oldfb = SelectObject(hdc, fb);
                    Rectangle(hdc, cmd.x1, cmd.y1, cmd.x2, cmd.y2);
                    SelectObject(hdc, oldfb);
                    DeleteObject(fb);
                    break;
                }
                case 5: { // fillcircle
                    HBRUSH fb = CreateSolidBrush(RGB((col>>16)&0xFF, (col>>8)&0xFF, col&0xFF));
                    HGDIOBJ oldfb = SelectObject(hdc, fb);
                    Ellipse(hdc, cmd.x1 - cmd.x2, cmd.y1 - cmd.x2, cmd.x1 + cmd.x2, cmd.y1 + cmd.x2);
                    SelectObject(hdc, oldfb);
                    DeleteObject(fb);
                    break;
                }
                case 6: { // image
                    HBITMAP img = (HBITMAP)LoadImageW(nullptr, s2ws(cmd.path).c_str(),
                                                      IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
                    if (img) {
                        HDC memdc = CreateCompatibleDC(hdc);
                        if (memdc) {
                            HGDIOBJ oldbm = SelectObject(memdc, img);
                            BITMAP bm;
                            GetObjectW(img, sizeof(bm), &bm);
                            BitBlt(hdc, cmd.x1, cmd.y1, bm.bmWidth, bm.bmHeight,
                                   memdc, 0, 0, SRCCOPY);
                            SelectObject(memdc, oldbm);
                            DeleteDC(memdc);
                        }
                        DeleteObject(img);
                    }
                    break;
                }
                }
                SelectObject(hdc, oldp);
                SelectObject(hdc, oldb);
                DeleteObject(pen);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: {
        auto hid_it = gui._handle_to_id.find(hwnd);
        if (hid_it != gui._handle_to_id.end()) {
            gui._handles.erase(hid_it->second);
            gui._handle_to_id.erase(hid_it);
        }
        auto bit_it = s_widget_brushes.find(hwnd);
        if (bit_it != s_widget_brushes.end()) {
            DeleteObject(bit_it->second);
            s_widget_brushes.erase(bit_it);
        }
        s_widget_bg_colors.erase(hwnd);
        s_canvas_cmds.erase(hwnd);
        auto bmp_it = s_widget_bitmaps.find(hwnd);
        if (bmp_it != s_widget_bitmaps.end()) {
            DeleteObject(bmp_it->second);
            s_widget_bitmaps.erase(bmp_it);
        }
        auto menu_it = s_window_menus.find(hwnd);
        if (menu_it != s_window_menus.end()) {
            SetMenu(hwnd, NULL);
            DestroyMenu(menu_it->second);
            s_window_menus.erase(menu_it);
        }
        if (!GetParent(hwnd)) PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}



WidgetId GuiSystem::create_window(const std::string& title, int width, int height) {
    register_classes();
    WidgetId id = next_id();
    HWND hwnd = CreateWindowExW(0, s_main_class, s2ws(title).c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    _handles[id] = hwnd;
    _handle_to_id[hwnd] = id;
    return id;
}

static WidgetId create_ctrl(GuiSystem& gui, WidgetId parent, const wchar_t* cls,
                            const std::string& text, int x, int y, int w, int h, DWORD style = 0) {
    HWND parent_hwnd = (HWND)gui.get_handle(parent);
    if (!parent_hwnd) return 0;
    WidgetId id = gui.next_id();
    DWORD base = WS_CHILD | WS_VISIBLE | style;
    HWND ctrl = CreateWindowExW(0, cls, s2ws(text).c_str(), base,
        x, y, w, h, parent_hwnd, (HMENU)id, GetModuleHandleW(nullptr), nullptr);
    gui._handles[id] = ctrl;
    gui._handle_to_id[ctrl] = id;
    return id;
}

WidgetId GuiSystem::create_button(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"BUTTON", text, x, y, w, h, BS_PUSHBUTTON);
}

WidgetId GuiSystem::create_label(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"STATIC", text, x, y, w, h, SS_LEFT);
}

WidgetId GuiSystem::create_textbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"EDIT", text, x, y, w, h, ES_AUTOHSCROLL);
}

WidgetId GuiSystem::create_checkbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"BUTTON", text, x, y, w, h, BS_CHECKBOX);
}

WidgetId GuiSystem::create_radio(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"BUTTON", text, x, y, w, h, BS_RADIOBUTTON);
}

WidgetId GuiSystem::create_listbox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) {
    WidgetId id = create_ctrl(*this, parent, L"LISTBOX", "", x, y, w, h, LBS_NOTIFY);
    HWND ctrl = (HWND)get_handle(id);
    for (auto& item : items) SendMessageW(ctrl, LB_ADDSTRING, 0, (LPARAM)s2ws(item).c_str());
    return id;
}

WidgetId GuiSystem::create_combobox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) {
    WidgetId id = create_ctrl(*this, parent, L"COMBOBOX", "", x, y, w, h, CBS_DROPDOWNLIST);
    HWND ctrl = (HWND)get_handle(id);
    for (auto& item : items) SendMessageW(ctrl, CB_ADDSTRING, 0, (LPARAM)s2ws(item).c_str());
    return id;
}

WidgetId GuiSystem::create_scrollbar(WidgetId parent, int x, int y, int w, int h) {
    return create_ctrl(*this, parent, L"SCROLLBAR", "", x, y, w, h, SBS_HORZ);
}

WidgetId GuiSystem::create_canvas(WidgetId parent, int x, int y, int w, int h) {
    HWND parent_hwnd = (HWND)get_handle(parent);
    if (!parent_hwnd) return 0;
    WidgetId id = next_id();
    HWND ctrl = CreateWindowExW(0, s_canvas_class, L"", WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent_hwnd, (HMENU)id, GetModuleHandleW(nullptr), nullptr);
    _handles[id] = ctrl;
    _handle_to_id[ctrl] = id;
    s_canvas_cmds[ctrl] = {};
    return id;
}

WidgetId GuiSystem::create_image(WidgetId parent, const std::string& path, int x, int y, int w, int h) {
    WidgetId id = create_ctrl(*this, parent, L"STATIC", "", x, y, w, h, SS_BITMAP);
    HWND ctrl = (HWND)get_handle(id);
    if (!ctrl) return id;
    HBITMAP bmp = (HBITMAP)LoadImageW(nullptr, s2ws(path).c_str(), IMAGE_BITMAP, w, h, LR_LOADFROMFILE);
    if (bmp) {
        HBITMAP old_bmp = (HBITMAP)SendMessageW(ctrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
        if (old_bmp) DeleteObject(old_bmp);
        s_widget_bitmaps[ctrl] = bmp;
    }
    return id;
}

void GuiSystem::create_menu(WidgetId parent, const std::vector<std::string>& items) {
    HWND hwnd = (HWND)get_handle(parent);
    if (!hwnd) return;
    auto old_it = s_window_menus.find(hwnd);
    if (old_it != s_window_menus.end()) {
        SetMenu(hwnd, NULL);
        DestroyMenu(old_it->second);
        s_window_menus.erase(old_it);
    }
    HMENU menu = CreateMenu();
    for (size_t i = 0; i < items.size(); ++i) {
        AppendMenuW(menu, MF_STRING, (UINT_PTR)(i + 1), s2ws(items[i]).c_str());
    }
    SetMenu(hwnd, menu);
    s_window_menus[hwnd] = menu;
}

int GuiSystem::show_dialog(const std::string& type, const std::string& title, const std::string& message) {
    UINT flags = MB_OK;
    if (type == "yesno") flags = MB_YESNO;
    else if (type == "okcancel") flags = MB_OKCANCEL;
    else if (type == "warning") flags = MB_OK | MB_ICONWARNING;
    else if (type == "error") flags = MB_OK | MB_ICONERROR;
    else if (type == "info") flags = MB_OK | MB_ICONINFORMATION;
    int ret = MessageBoxW(nullptr, s2ws(message).c_str(), s2ws(title).c_str(), flags);
    if (ret == IDYES || ret == IDOK) return 1;
    return 0;
}

void GuiSystem::show(WidgetId window) {
    HWND hwnd = (HWND)get_handle(window);
    if (hwnd) ShowWindow(hwnd, SW_SHOW);
}

void GuiSystem::close(WidgetId window) {
    HWND hwnd = (HWND)get_handle(window);
    if (hwnd) DestroyWindow(hwnd);
}

void GuiSystem::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!s_pending_callback.empty()) {
            std::string fn = s_pending_callback;
            WidgetId wid = s_pending_widget_id;
            s_pending_callback.clear();
            s_pending_widget_id = 0;
            GuiSystem::instance().dispatch(fn);
        }
    }
}

void GuiSystem::set_text(WidgetId widget, const std::string& text) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) SetWindowTextW(ctrl, s2ws(text).c_str());
}

std::string GuiSystem::get_text(WidgetId widget) {
    HWND ctrl = (HWND)get_handle(widget);
    if (!ctrl) return "";
    int len = GetWindowTextLengthW(ctrl) + 1;
    std::wstring ws(len, 0);
    GetWindowTextW(ctrl, &ws[0], len);
    ws.resize(GetWindowTextLengthW(ctrl));
    return ws2s(ws);
}

void GuiSystem::set_pos(WidgetId widget, int x, int y) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) {
        RECT r;
        GetWindowRect(ctrl, &r);
        SetWindowPos(ctrl, nullptr, x, y, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
    }
}

void GuiSystem::set_size(WidgetId widget, int w, int h) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) SetWindowPos(ctrl, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
}

void GuiSystem::set_color(WidgetId widget, const std::string& color) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) {
        uint32_t col = parse_color(color);
        COLORREF cr = RGB((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
        s_widget_bg_colors[ctrl] = cr;
        auto it = s_widget_brushes.find(ctrl);
        if (it != s_widget_brushes.end()) DeleteObject(it->second);
        s_widget_brushes[ctrl] = CreateSolidBrush(cr);
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::set_visible(WidgetId widget, bool visible) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) ShowWindow(ctrl, visible ? SW_SHOW : SW_HIDE);
}

void GuiSystem::set_enabled(WidgetId widget, bool enabled) {
    HWND ctrl = (HWND)get_handle(widget);
    if (ctrl) EnableWindow(ctrl, enabled ? TRUE : FALSE);
}

void GuiSystem::draw_line(WidgetId canvas, int x1, int y1, int x2, int y2, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({0, x1, y1, x2, y2, "", color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::draw_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({1, x, y, x + w, y + h, "", color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::draw_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({2, cx, cy, r, 0, "", color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::draw_text(WidgetId canvas, const std::string& text, int x, int y, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({3, x, y, 0, 0, text, color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::draw_image(WidgetId canvas, const std::string& path, int x, int y) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({6, x, y, 0, 0, "", "", path});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::fill_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({4, x, y, x + w, y + h, "", color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::fill_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].push_back({5, cx, cy, r, 0, "", color, ""});
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::clear_canvas(WidgetId canvas) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) {
        s_canvas_cmds[ctrl].clear();
        InvalidateRect(ctrl, nullptr, TRUE);
    }
}

void GuiSystem::update_canvas(WidgetId canvas) {
    HWND ctrl = (HWND)get_handle(canvas);
    if (ctrl) InvalidateRect(ctrl, nullptr, TRUE);
}

// =====================================================================
// Linux X11 实现 (基础窗口/按钮可用，其余未实现)
// =====================================================================
#elif defined(__linux__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display* s_display = nullptr;
static int s_screen = 0;

static void ensure_display() {
    if (!s_display) {
        s_display = XOpenDisplay(nullptr);
        s_screen = DefaultScreen(s_display);
    }
}

[[noreturn]] static void gui_not_impl() {
    throw std::runtime_error("RUN013: GUI 此功能在 Linux/X11 上尚未实现");
}

WidgetId GuiSystem::create_window(const std::string& title, int width, int height) {
    ensure_display();
    if (!s_display) return 0;
    WidgetId id = next_id();
    Window win = XCreateSimpleWindow(s_display, RootWindow(s_display, s_screen),
        0, 0, width, height, 1, BlackPixel(s_display, s_screen), WhitePixel(s_display, s_screen));
    XStoreName(s_display, win, title.c_str());
    XSelectInput(s_display, win, ExposureMask | KeyPressMask | ButtonPressMask);
    _handles[id] = (void*)win;
    return id;
}

WidgetId GuiSystem::create_button(WidgetId parent, const std::string& text, int x, int y, int w, int h) {
    ensure_display();
    Window parent_win = (Window)_handles[parent];
    if (!parent_win) return 0;
    WidgetId id = next_id();
    Window btn = XCreateSimpleWindow(s_display, parent_win, x, y, w, h, 1,
        BlackPixel(s_display, s_screen), WhitePixel(s_display, s_screen));
    XStoreName(s_display, btn, text.c_str());
    _handles[id] = (void*)btn;
    return id;
}

WidgetId GuiSystem::create_label(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_textbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_checkbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_radio(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_listbox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_combobox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_scrollbar(WidgetId parent, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_canvas(WidgetId parent, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_image(WidgetId parent, const std::string& path, int x, int y, int w, int h) { gui_not_impl(); }

void GuiSystem::create_menu(WidgetId parent, const std::vector<std::string>& items) { gui_not_impl(); }
int GuiSystem::show_dialog(const std::string& type, const std::string& title, const std::string& message) { gui_not_impl(); }
void GuiSystem::show(WidgetId window) { if (s_display) XMapWindow(s_display, (Window)_handles[window]); }
void GuiSystem::close(WidgetId window) { if (s_display) XDestroyWindow(s_display, (Window)_handles[window]); }
void GuiSystem::run() {
    if (!s_display) return;
    XEvent ev;
    while (1) { XNextEvent(s_display, &ev); if (ev.type == KeyPress) break; }
}
void GuiSystem::set_text(WidgetId widget, const std::string& text) { gui_not_impl(); }
std::string GuiSystem::get_text(WidgetId widget) { gui_not_impl(); }
void GuiSystem::set_pos(WidgetId widget, int x, int y) { gui_not_impl(); }
void GuiSystem::set_size(WidgetId widget, int w, int h) { gui_not_impl(); }
void GuiSystem::set_color(WidgetId widget, const std::string& color) { gui_not_impl(); }
void GuiSystem::set_visible(WidgetId widget, bool visible) { gui_not_impl(); }
void GuiSystem::set_enabled(WidgetId widget, bool enabled) { gui_not_impl(); }
void GuiSystem::draw_line(WidgetId canvas, int x1, int y1, int x2, int y2, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_text(WidgetId canvas, const std::string& text, int x, int y, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_image(WidgetId canvas, const std::string& path, int x, int y) { gui_not_impl(); }
void GuiSystem::fill_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) { gui_not_impl(); }
void GuiSystem::fill_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) { gui_not_impl(); }
void GuiSystem::clear_canvas(WidgetId canvas) { gui_not_impl(); }
void GuiSystem::update_canvas(WidgetId canvas) { gui_not_impl(); }

// =====================================================================
// macOS Cocoa 实现 (未实现，需 Objective-C runtime + .mm 文件)
// =====================================================================
#elif defined(__APPLE__)

[[noreturn]] static void gui_not_impl() {
    throw std::runtime_error("RUN013: GUI 在 macOS 上尚未实现，需要 Objective-C Cocoa 绑定");
}

WidgetId GuiSystem::create_window(const std::string& title, int width, int height) { gui_not_impl(); }
WidgetId GuiSystem::create_button(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_label(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_textbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_checkbox(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_radio(WidgetId parent, const std::string& text, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_listbox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_combobox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_scrollbar(WidgetId parent, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_canvas(WidgetId parent, int x, int y, int w, int h) { gui_not_impl(); }
WidgetId GuiSystem::create_image(WidgetId parent, const std::string& path, int x, int y, int w, int h) { gui_not_impl(); }
void GuiSystem::create_menu(WidgetId parent, const std::vector<std::string>& items) { gui_not_impl(); }
int GuiSystem::show_dialog(const std::string& type, const std::string& title, const std::string& message) { gui_not_impl(); }
void GuiSystem::show(WidgetId window) { gui_not_impl(); }
void GuiSystem::close(WidgetId window) { gui_not_impl(); }
void GuiSystem::run() { gui_not_impl(); }
void GuiSystem::set_text(WidgetId widget, const std::string& text) { gui_not_impl(); }
std::string GuiSystem::get_text(WidgetId widget) { gui_not_impl(); }
void GuiSystem::set_pos(WidgetId widget, int x, int y) { gui_not_impl(); }
void GuiSystem::set_size(WidgetId widget, int w, int h) { gui_not_impl(); }
void GuiSystem::set_color(WidgetId widget, const std::string& color) { gui_not_impl(); }
void GuiSystem::set_visible(WidgetId widget, bool visible) { gui_not_impl(); }
void GuiSystem::set_enabled(WidgetId widget, bool enabled) { gui_not_impl(); }
void GuiSystem::draw_line(WidgetId canvas, int x1, int y1, int x2, int y2, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_text(WidgetId canvas, const std::string& text, int x, int y, const std::string& color) { gui_not_impl(); }
void GuiSystem::draw_image(WidgetId canvas, const std::string& path, int x, int y) { gui_not_impl(); }
void GuiSystem::fill_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color) { gui_not_impl(); }
void GuiSystem::fill_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color) { gui_not_impl(); }
void GuiSystem::clear_canvas(WidgetId canvas) { gui_not_impl(); }
void GuiSystem::update_canvas(WidgetId canvas) { gui_not_impl(); }

#endif

// ===== GUI 内置函数注册 =====
using GuiBuiltinFn = std::function<Value(std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&)>;

static inline void gui_rb(BuiltinRegistry& reg, const char* name, int min_a, int max_a,
                          const char* ret_type, GuiBuiltinFn fn) {
    BuiltinInfo info;
    info.name = name;
    info.min_arity = min_a;
    info.max_arity = max_a;
    info.return_type = ret_type;
    info.eval_fn = std::move(fn);
    reg.register_builtin(info);
}

void register_all_gui() {
    auto& reg = BuiltinRegistry::instance();
    auto& gui = GuiSystem::instance();

    static auto check_num_args = [](const std::vector<Value>& a, const char* fn_name, int start, int end) {
        for (int i = start; i <= end && i < static_cast<int>(a.size()); ++i) {
            if (!a[i].is_int() && !a[i].is_float())
                throw std::runtime_error(std::string("RUN332: ") + fn_name + " 参数 " + std::to_string(i+1) + " 期望数值");
        }
    };

    gui_rb(reg, "gui_window", 2, 3, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_string()) throw std::runtime_error("RUN152: gui_window 标题参数期望字符串");
        if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN152: gui_window 宽度参数期望数值");
        if (a.size() > 2 && !a[2].is_int() && !a[2].is_float()) throw std::runtime_error("RUN332: gui_window 高度参数期望数值");
        return Value::make_int(gui.create_window(a[0].as_string(), (int)a[1].as_number(),
            a.size() > 2 ? (int)a[2].as_number() : 400));
    });

    gui_rb(reg, "gui_button", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN153: gui_button 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN153: gui_button 文本参数期望字符串");
        check_num_args(a, "gui_button", 2, 5);
        return Value::make_int(gui.create_button(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_label", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN154: gui_label 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN154: gui_label 文本参数期望字符串");
        check_num_args(a, "gui_label", 2, 5);
        return Value::make_int(gui.create_label(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_textbox", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN155: gui_textbox 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN155: gui_textbox 文本参数期望字符串");
        check_num_args(a, "gui_textbox", 2, 5);
        return Value::make_int(gui.create_textbox(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_checkbox", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN156: gui_checkbox 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN156: gui_checkbox 文本参数期望字符串");
        check_num_args(a, "gui_checkbox", 2, 5);
        return Value::make_int(gui.create_checkbox(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_radio", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN157: gui_radio 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN157: gui_radio 文本参数期望字符串");
        check_num_args(a, "gui_radio", 2, 5);
        return Value::make_int(gui.create_radio(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_listbox", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN158: gui_listbox 父窗口ID期望整数");
        std::vector<std::string> items;
        if (a[1].is_array()) for (auto& v : a[1].as_array()) items.push_back(v.to_display());
        check_num_args(a, "gui_listbox", 2, 5);
        return Value::make_int(gui.create_listbox(a[0].as_int(), items,
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_combobox", 6, 6, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN159: gui_combobox 父窗口ID期望整数");
        std::vector<std::string> items;
        if (a[1].is_array()) for (auto& v : a[1].as_array()) items.push_back(v.to_display());
        check_num_args(a, "gui_combobox", 2, 5);
        return Value::make_int(gui.create_combobox(a[0].as_int(), items,
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_scrollbar", 5, 5, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN160: gui_scrollbar 父窗口ID期望整数");
        check_num_args(a, "gui_scrollbar", 1, 4);
        return Value::make_int(gui.create_scrollbar(a[0].as_int(),
            (int)a[1].as_number(), (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number()));
    });

    gui_rb(reg, "gui_canvas", 5, 5, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN161: gui_canvas 父窗口ID期望整数");
        check_num_args(a, "gui_canvas", 1, 4);
        return Value::make_int(gui.create_canvas(a[0].as_int(),
            (int)a[1].as_number(), (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number()));
    });

    gui_rb(reg, "gui_image", 7, 7, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN162: gui_image 父窗口ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN162: gui_image 路径参数期望字符串");
        check_num_args(a, "gui_image", 2, 5);
        return Value::make_int(gui.create_image(a[0].as_int(), a[1].as_string(),
            (int)a[2].as_number(), (int)a[3].as_number(), (int)a[4].as_number(), (int)a[5].as_number()));
    });

    gui_rb(reg, "gui_menu", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN163: gui_menu 父窗口ID期望整数");
        std::vector<std::string> items;
        if (a[1].is_array()) for (auto& v : a[1].as_array()) items.push_back(v.to_display());
        gui.create_menu(a[0].as_int(), items);
        return Value::make_null();
    });

    gui_rb(reg, "gui_dialog", 3, 3, "int", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_string() || !a[1].is_string() || !a[2].is_string()) throw std::runtime_error("RUN164: gui_dialog 参数期望字符串");
        return Value::make_int(gui.show_dialog(a[0].as_string(), a[1].as_string(), a[2].as_string()));
    });

    gui_rb(reg, "gui_on_click", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN165: gui_on_click 控件ID期望整数");
        if (a[1].is_fnref()) gui.on_click(a[0].as_int(), a[1].as_fnref().name);
        else if (a[1].is_string()) gui.on_click(a[0].as_int(), a[1].as_string());
        else throw std::runtime_error("RUN035: gui_on_click 回调参数必须为函数或字符串");
        return Value::make_null();
    });

    gui_rb(reg, "gui_on_change", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN166: gui_on_change 控件ID期望整数");
        if (a[1].is_fnref()) gui.on_change(a[0].as_int(), a[1].as_fnref().name);
        else if (a[1].is_string()) gui.on_change(a[0].as_int(), a[1].as_string());
        else throw std::runtime_error("RUN036: gui_on_change 回调参数必须为函数或字符串");
        return Value::make_null();
    });

    gui_rb(reg, "gui_show", 1, 1, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN167: gui_show 窗口ID期望整数");
        gui.show(a[0].as_int()); return Value::make_null();
    });

    gui_rb(reg, "gui_close", 1, 1, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN168: gui_close 窗口ID期望整数");
        gui.close(a[0].as_int()); return Value::make_null();
    });

    gui_rb(reg, "gui_trigger", 1, 1, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN169: gui_trigger 控件ID期望整数");
        WidgetId wid = a[0].as_int();
        if (gui.has_callback(wid)) {
            gui.dispatch(gui.get_callback(wid).fn_name);
        }
        return Value::make_null();
    });

    gui_rb(reg, "gui_run", 0, 0, "void", [&](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
        gui.run(); return Value::make_null();
    });

    gui_rb(reg, "gui_set_text", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN170: gui_set_text 控件ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN170: gui_set_text 文本参数期望字符串");
        gui.set_text(a[0].as_int(), a[1].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_get_text", 1, 1, "string", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN171: gui_get_text 控件ID期望整数");
        return Value::make_string(gui.get_text(a[0].as_int()));
    });

    gui_rb(reg, "gui_set_pos", 3, 3, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN172: gui_set_pos 控件ID期望整数");
        check_num_args(a, "gui_set_pos", 1, 2);
        gui.set_pos(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number()); return Value::make_null();
    });

    gui_rb(reg, "gui_set_size", 3, 3, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN173: gui_set_size 控件ID期望整数");
        check_num_args(a, "gui_set_size", 1, 2);
        gui.set_size(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number()); return Value::make_null();
    });

    gui_rb(reg, "gui_set_color", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN174: gui_set_color 控件ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN174: gui_set_color 颜色参数期望字符串");
        gui.set_color(a[0].as_int(), a[1].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_set_visible", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN175: gui_set_visible 控件ID期望整数");
        if (!a[1].is_bool()) throw std::runtime_error("RUN053: gui_set_visible 第二参数必须为布尔值");
        gui.set_visible(a[0].as_int(), a[1].as_bool()); return Value::make_null();
    });

    gui_rb(reg, "gui_set_enabled", 2, 2, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN176: gui_set_enabled 控件ID期望整数");
        if (!a[1].is_bool()) throw std::runtime_error("RUN054: gui_set_enabled 第二参数必须为布尔值");
        gui.set_enabled(a[0].as_int(), a[1].as_bool()); return Value::make_null();
    });

    gui_rb(reg, "gui_draw_line", 6, 6, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN177: gui_draw_line 画布ID期望整数");
        check_num_args(a, "gui_draw_line", 1, 4);
        if (!a[5].is_string()) throw std::runtime_error("RUN177: gui_draw_line 颜色参数期望字符串");
        gui.draw_line(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number(),
            (int)a[3].as_number(), (int)a[4].as_number(), a[5].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_draw_rect", 6, 6, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN178: gui_draw_rect 画布ID期望整数");
        check_num_args(a, "gui_draw_rect", 1, 4);
        if (!a[5].is_string()) throw std::runtime_error("RUN178: gui_draw_rect 颜色参数期望字符串");
        gui.draw_rect(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number(),
            (int)a[3].as_number(), (int)a[4].as_number(), a[5].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_draw_circle", 5, 5, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN179: gui_draw_circle 画布ID期望整数");
        check_num_args(a, "gui_draw_circle", 1, 3);
        if (!a[4].is_string()) throw std::runtime_error("RUN179: gui_draw_circle 颜色参数期望字符串");
        gui.draw_circle(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number(),
            (int)a[3].as_number(), a[4].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_draw_text", 5, 5, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN180: gui_draw_text 画布ID期望整数");
        if (!a[1].is_string()) throw std::runtime_error("RUN180: gui_draw_text 文本参数期望字符串");
        check_num_args(a, "gui_draw_text", 2, 3);
        if (!a[4].is_string()) throw std::runtime_error("RUN180: gui_draw_text 颜色参数期望字符串");
        gui.draw_text(a[0].as_int(), a[1].as_string(), (int)a[2].as_number(),
            (int)a[3].as_number(), a[4].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_fill_rect", 6, 6, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN181: gui_fill_rect 画布ID期望整数");
        check_num_args(a, "gui_fill_rect", 1, 4);
        if (!a[5].is_string()) throw std::runtime_error("RUN181: gui_fill_rect 颜色参数期望字符串");
        gui.fill_rect(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number(),
            (int)a[3].as_number(), (int)a[4].as_number(), a[5].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_fill_circle", 5, 5, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN182: gui_fill_circle 画布ID期望整数");
        check_num_args(a, "gui_fill_circle", 1, 3);
        if (!a[4].is_string()) throw std::runtime_error("RUN182: gui_fill_circle 颜色参数期望字符串");
        gui.fill_circle(a[0].as_int(), (int)a[1].as_number(), (int)a[2].as_number(),
            (int)a[3].as_number(), a[4].as_string()); return Value::make_null();
    });

    gui_rb(reg, "gui_clear_canvas", 1, 1, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN183: gui_clear_canvas 画布ID期望整数");
        gui.clear_canvas(a[0].as_int()); return Value::make_null();
    });

    gui_rb(reg, "gui_update", 1, 1, "void", [&](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_int()) throw std::runtime_error("RUN184: gui_update 画布ID期望整数");
        gui.update_canvas(a[0].as_int()); return Value::make_null();
    });
}

} // namespace next11