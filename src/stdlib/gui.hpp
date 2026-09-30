#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <any>
#include <cstdint>

namespace next11 {

using WidgetId = int64_t;

struct GuiCallback {
    std::string fn_name;
    WidgetId widget_id;
};

class GuiSystem {
public:
    static GuiSystem& instance();

    WidgetId create_window(const std::string& title, int width, int height);
    WidgetId create_button(WidgetId parent, const std::string& text, int x, int y, int w, int h);
    WidgetId create_label(WidgetId parent, const std::string& text, int x, int y, int w, int h);
    WidgetId create_textbox(WidgetId parent, const std::string& text, int x, int y, int w, int h);
    WidgetId create_checkbox(WidgetId parent, const std::string& text, int x, int y, int w, int h);
    WidgetId create_radio(WidgetId parent, const std::string& text, int x, int y, int w, int h);
    WidgetId create_listbox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h);
    WidgetId create_combobox(WidgetId parent, const std::vector<std::string>& items, int x, int y, int w, int h);
    WidgetId create_scrollbar(WidgetId parent, int x, int y, int w, int h);
    WidgetId create_canvas(WidgetId parent, int x, int y, int w, int h);
    WidgetId create_image(WidgetId parent, const std::string& path, int x, int y, int w, int h);

    void create_menu(WidgetId parent, const std::vector<std::string>& items);
    int show_dialog(const std::string& type, const std::string& title, const std::string& message);

    void on_click(WidgetId widget, const std::string& fn_name);
    void on_change(WidgetId widget, const std::string& fn_name);

    void show(WidgetId window);
    void close(WidgetId window);
    void run();

    void set_text(WidgetId widget, const std::string& text);
    std::string get_text(WidgetId widget);
    void set_pos(WidgetId widget, int x, int y);
    void set_size(WidgetId widget, int w, int h);
    void set_color(WidgetId widget, const std::string& color);
    void set_visible(WidgetId widget, bool visible);
    void set_enabled(WidgetId widget, bool enabled);

    void draw_line(WidgetId canvas, int x1, int y1, int x2, int y2, const std::string& color);
    void draw_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color);
    void draw_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color);
    void draw_text(WidgetId canvas, const std::string& text, int x, int y, const std::string& color);
    void draw_image(WidgetId canvas, const std::string& path, int x, int y);
    void fill_rect(WidgetId canvas, int x, int y, int w, int h, const std::string& color);
    void fill_circle(WidgetId canvas, int cx, int cy, int r, const std::string& color);
    void clear_canvas(WidgetId canvas);
    void update_canvas(WidgetId canvas);

    bool has_callback(WidgetId widget);
    GuiCallback get_callback(WidgetId widget);
    bool has_change_callback(WidgetId widget);
    GuiCallback get_change_callback(WidgetId widget);

    void set_interpreter(void* interp) { _interp = interp; }
    void* get_interpreter() { return _interp; }
    void set_dispatcher(std::function<void(const std::string&)> d) { _dispatcher = std::move(d); }
    void dispatch(const std::string& fn_name) { if (_dispatcher) _dispatcher(fn_name); }

    bool is_valid(WidgetId id);
    WidgetId next_id() { return ++_id_counter; }
    void* get_handle(WidgetId id);
    WidgetId get_id(void* handle);

    int64_t _id_counter = 0;
    void* _interp = nullptr;
    std::function<void(const std::string&)> _dispatcher;
    std::map<WidgetId, GuiCallback> _click_callbacks;
    std::map<WidgetId, GuiCallback> _change_callbacks;
    std::map<WidgetId, void*> _handles;
    std::map<void*, WidgetId> _handle_to_id;
};

void register_all_gui();

} // namespace next11