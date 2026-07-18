#include <gtkmm.h>
#include <libayatana-appindicator/app-indicator.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <regex>
#include <cstring>

class MyWindow : public Gtk::Window {
public:
    MyWindow() {
        if(auto app = Gtk::Application::get_default()) {
            app->hold();
        }
        // 1. 启用透明支持
        auto screen = Gdk::Screen::get_default();
        auto visual = screen->get_rgba_visual();
        if (visual) gtk_widget_set_visual(GTK_WIDGET(this->gobj()), visual->gobj());

        // 2. 窗口物理大小固定（感应区：宽 600, 高 200）
        const int WIN_W = 350;
        const int WIN_H = 180;
        set_default_size(WIN_W, WIN_H);
        set_size_request(WIN_W, WIN_H);

        set_decorated(false);
        set_resizable(false);
        set_app_paintable(true);
        set_type_hint(Gdk::WINDOW_TYPE_HINT_DOCK);
        set_keep_above(true);
        set_accept_focus(false);

        auto* overlay = Gtk::make_managed<Gtk::Overlay>();
        overlay->add(drawing_area);

        label.set_markup("<span color='white' font='11'><b>← GSConnect  |  LocalSend →</b></span>");
        label.set_opacity(0.0);
        label.set_valign(Gtk::Align::ALIGN_START); // 改为顶部对齐
        label.set_halign(Gtk::Align::ALIGN_CENTER); // 水平依然居中
        overlay->add_overlay(label);
        add(*overlay);

        drawing_area.signal_draw().connect(sigc::mem_fun(*this, &MyWindow::on_drawing_area_draw));

        // 3. 拖放感应区逻辑
        std::vector<Gtk::TargetEntry> targets = {
            Gtk::TargetEntry("text/uri-list", Gtk::TARGET_OTHER_APP),
            Gtk::TargetEntry("UTF8_STRING")
        };
        drag_dest_set(targets, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
        // 在构造函数内
        // ... 之前的 drag_dest_set 之后 ...

        signal_drag_data_received().connect([this](const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time) {

            int win_w = drawing_area.get_allocated_width();
            double island_x_start = (win_w - current_w) / 2.0;

            if (x < island_x_start || x > (island_x_start + current_w) || y > (current_h + current_y)) {
                std::cout << "释放点在感应区但在岛屿外，忽略发送。" << std::endl;

                // 收缩岛屿并收回
                target_w = 100.0;
                target_h = 20.0;
                label.set_opacity(0.0);
                start_animation();

                context->drag_finish(false, false, time);
                return;
            }

            // 1. 解析拖入的数据
            std::vector<std::string> paths;
            if ((selection_data.get_length() >= 0) && (selection_data.get_format() == 8)) {
                // 处理 URI 列表 (文件和文件夹)
                auto uris = selection_data.get_uris();
                if (!uris.empty()) {
                    for (auto& uri : uris) {
                        // 将 file:// 协议转换为本地绝对路径
                        auto file = Gio::File::create_for_uri(uri);
                        std::string path = file->get_path();
                        if (!path.empty()) {
                            paths.push_back(path);
                        }
                    }
                } else {
                    // 如果不是文件，尝试获取纯文本内容
                    std::string text = selection_data.get_text();
                    if (!text.empty()) {
                        paths.push_back(text);
                    }
                }
            }

            // 2. 如果路径列表不为空，发送文件
            if (!paths.empty()) {
                if (drag_is_gsconnect && gsconnect_available) {
                    send_via_gsconnect(paths);
                    drag_is_gsconnect = false;
                } else {
                    std::string command = "localsend_app";
                    for (const auto& path : paths) {
                        command += " \"" + path + "\"";
                    }
                    command += " &";
                    std::cout << "LocalSend 启动中: " << command << std::endl;
                    std::system(command.c_str());
                }

                target_w = 100.0;
                target_h = 20.0;
                label.set_opacity(0.0);
                show_device_list = false;
                gsconnect_hovered = -1;
                target_fill = 0.0;
                target_scroll = 0.0;
                start_animation();
            }

            // 告诉系统拖放已完成
            context->drag_finish(true, false, time);
        });


        signal_drag_motion().connect([this](const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time) {
            cancel_auto_disable_timer();
            target_w = 300.0;
            target_h = 100.0;
            target_y = 30.0;
            label.set_opacity(1.0);
            start_animation();

            int win_w = drawing_area.get_allocated_width();
            double island_x_start = (win_w - current_w) / 2.0;

            if (x >= island_x_start && x <= (island_x_start + current_w) && y <= (current_h + current_y)) {
                gdk_drag_status(context->gobj(), GDK_ACTION_COPY, time);

                double island_center = island_x_start + current_w / 2.0;

                if (show_device_list) {
                    target_fill = -1.0;
                    drag_is_gsconnect = true;
                    int total = (int)gsconnect_devices.size();
                    double pad = fill_padding;
                    int vis = std::min(total, 3);
                    double inner_gap = (vis == 1) ? 0.0 : 6.0;
                    double btn_w = (current_w - pad * 2 - inner_gap * (vis - 1)) / vis;
                    double step = btn_w + inner_gap;

                    if (total > 3) {
                        double frac = (x - island_x_start) / current_w;
                        frac = std::clamp(frac, 0.0, 1.0);
                        double max_s = (double)(total - 3);
                        scroll_max = max_s;
                        double edge = 0.35;
                        if (frac < edge)
                            scroll_rate = -((edge - frac) / edge) * 0.22;
                        else if (frac > 1.0 - edge)
                            scroll_rate = ((frac - (1.0 - edge)) / edge) * 0.22;
                        else
                            scroll_rate = 0.0;
                    }

                    int sbase = (int)std::round(scroll_offset);
                    int idx = std::clamp(
                        (int)((x - island_x_start - pad + scroll_offset * step) / step + 0.5),
                        0, total - 1);
                    gsconnect_hovered = idx;
                    gsconnect_device = gsconnect_devices[idx];
                    label.set_markup(
                        "<span color='white' font='11'><b></b></span>");
                } else {
                    double offset = (x - island_center) / current_w;
                    double dead = 0.10;
                    if (std::abs(offset) < dead) {
                        target_fill = 0.0;
                    } else {
                        double remap = (std::abs(offset) - dead) / (0.5 - dead);
                        target_fill = (offset < 0 ? -1.0 : 1.0) * std::min(remap / 0.55, 1.0);
                    }
                    {
                        double af = std::abs(target_fill);
                        if (af >= 0.92 && prev_abs_fill < 0.92)
                            bounce_vel = 0.18;
                        prev_abs_fill = af;
                    }

                    if (offset < -dead) {
                        if (!gsconnect_checked)
                            query_gsconnect_devices();
                        if (gsconnect_available) {
                            drag_is_gsconnect = true;
                            if (std::abs(target_fill) >= 0.92 && !gsconnect_devices.empty()) {
                                show_device_list = true;
                                gsconnect_checked = false;
                            }
                            label.set_markup(
                                "<span color='white' font='11'><b>GSConnect</b></span>");
                        } else {
                            label.set_markup(
                                "<span color='white' font='11'><b>LocalSend</b></span>");
                        }
                    } else if (offset > dead) {
                        drag_is_gsconnect = false;
                        label.set_markup(
                            "<span color='white' font='11'><b>LocalSend</b></span>");
                    }
                }
            } else {
                gdk_drag_status(context->gobj(), (GdkDragAction)0, time);
                target_fill = 0.0;
                show_device_list = false;
                gsconnect_hovered = -1;
                target_scroll = 0.0;
            }
            return true;
        });


        signal_drag_leave().connect([this](const Glib::RefPtr<Gdk::DragContext>&, guint) {
            target_w = 100.0;
            target_h = 20.0;
            target_y = -50.0;
            label.set_opacity(0.0);
            label.set_markup("<span color='white' font='11'><b>← GSConnect  |  LocalSend →</b></span>");
            show_device_list = false;
            gsconnect_hovered = -1;
            gsconnect_checked = false;
            target_fill = 0.0;
            bounce_pos = 0.0;
            bounce_vel = 0.0;
            prev_abs_fill = 0.0;
            target_scroll = 0.0;
            start_animation();
            start_auto_disable_timer();
        });

        // 右键退出
        add_events(Gdk::BUTTON_PRESS_MASK);
        signal_button_press_event().connect([this](GdkEventButton* event) {
            if (event->button == 3){
                this->is_enabled = false;
                this->hide();
                update_app_indicator_icon();
                cancel_auto_disable_timer();
                send_status_changed_signal(false,"右键禁用");
                std::cout << "已禁用" << std::endl;
            }
            return true;
        });

        show_all_children();
        hide();

        // 4. 定位窗口位置（物理窗口位置固定不动）
        signal_realize().connect([this, WIN_W]() {
            auto screen = Gdk::Screen::get_default();
            Gdk::Rectangle rect;
            screen->get_monitor_geometry(screen->get_primary_monitor(), rect);
            move(rect.get_x() + (rect.get_width() - WIN_W) / 2, rect.get_y()+50);
            this->hide();
            send_status_changed_signal(false,"启动时禁用");
        });
        // 阻止窗口关闭时退出程序
        signal_delete_event().connect([this](GdkEventAny* event) {
            // 隐藏窗口但不退出程序
            this->hide();
            // 返回true表示我们已经处理了该事件，阻止默认的关闭行为
            return true;
        });
        create_app_indicator();
        // 初始化DBus连接
        setup_dbus();
    }
    ~MyWindow() {
        if (app_indicator) {
            // 清理AppIndicator资源
            g_object_unref(app_indicator);
        }
        if (dbus_connection) {
            g_object_unref(dbus_connection);
        }
    }

private:
    Gtk::Label label;
    Gtk::DrawingArea drawing_area;
    const double stiffness = 0.18, damping = 0.70;
    double current_h = 20.0, target_h = 20.0, vel_h = 0.0;
    double current_w = 100.0, target_w = 100.0, vel_w = 0.0;
    double current_y = -50.0, target_y = -50.0, vel_y = 0.0;
    double current_fill = 0.0, target_fill = 0.0, vel_fill = 0.0;
    double prev_abs_fill = 0.0, bounce_pos = 0.0, bounce_vel = 0.0;
    const double fill_padding = 4.0;
    sigc::connection tick_conn;
    sigc::connection auto_disable_conn;

    // 使用 Ayatana AppIndicator
    AppIndicator* app_indicator;
    bool is_enabled = false;

    // DBus相关
    GDBusConnection* dbus_connection;
    guint dbus_registration_id;

    // GSConnect 相关
    struct GSConnectDevice {
        std::string id;
        std::string name;
        bool connected = false;
    };
    bool drag_is_gsconnect = false;
    GSConnectDevice gsconnect_device;       // 当前选中的 GSConnect 设备
    bool gsconnect_checked = false;
    bool gsconnect_available = false;
    std::vector<GSConnectDevice> gsconnect_devices;  // 所有设备列表
    bool show_device_list = false;           // 是否展示设备列表
    int gsconnect_hovered = -1;
    double scroll_offset = 0.0, target_scroll = 0.0, scroll_vel = 0.0;
    double scroll_max = 0.0;
    double scroll_rate = 0.0;

    void setup_dbus() {
        GError* error = nullptr;

        // 连接到会话总线
        dbus_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (error) {
            std::cerr << "DBus连接失败: " << error->message << std::endl;
            g_error_free(error);
            return;
        }

        // 注册DBus服务名称
        guint name_id = g_bus_own_name_on_connection(
            dbus_connection,
            "com.kechen.island2localsend",
            G_BUS_NAME_OWNER_FLAGS_NONE,
            nullptr,  // 名称获取回调
            nullptr,  // 名称丢失回调
            nullptr,  // 用户数据
            nullptr   // 用户数据释放函数
        );

        // 创建接口信息
        GDBusNodeInfo* introspection_data = nullptr;
        const gchar introspection_xml[] =
            "<node>"
            "  <interface name='com.kechen.island2localsend'>"
            "    <method name='Enable'>"
            "      <arg type='b' name='show_window' direction='in'/>"
            "      <arg type='s' name='message' direction='in'/>"
            "      <arg type='b' name='success' direction='out'/>"
            "    </method>"
            "    <method name='ShowNotification'>"
            "      <arg type='s' name='title' direction='in'/>"
            "      <arg type='s' name='message' direction='in'/>"
            "      <arg type='b' name='success' direction='out'/>"
            "    </method>"
            "    <signal name='StatusChanged'>"
            "      <arg type='b' name='enabled'/>"
            "      <arg type='s' name='status_message'/>"
            "    </signal>"
            "  </interface>"
            "</node>";

        introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
        if (error) {
            std::cerr << "DBus接口解析失败: " << error->message << std::endl;
            g_error_free(error);
            return;
        }

        // 获取接口信息
        GDBusInterfaceInfo* interface_info = g_dbus_node_info_lookup_interface(
            introspection_data, "com.kechen.island2localsend");

        // 接口虚拟函数表
        static const GDBusInterfaceVTable interface_vtable = {
            handle_method_call,
            nullptr,  // 属性获取
            nullptr   // 属性设置
        };

        // 注册对象
        dbus_registration_id = g_dbus_connection_register_object(
            dbus_connection,
            "/com/kechen/island2localsend",
            interface_info,
            &interface_vtable,
            this,  // 用户数据，指向MyWindow实例
            nullptr,  // 用户数据释放函数
            &error);

        if (error) {
            std::cerr << "DBus对象注册失败: " << error->message << std::endl;
            g_error_free(error);
            g_dbus_node_info_unref(introspection_data);
            return;
        }

        std::cout << "DBus服务已启动: com.kechen.island2localsend" << std::endl;

        // 释放接口信息
        g_dbus_node_info_unref(introspection_data);
    }

    // 静态方法，处理DBus方法调用
    static void handle_method_call(GDBusConnection* connection,
                                    const gchar* sender,
                                    const gchar* object_path,
                                    const gchar* interface_name,
                                    const gchar* method_name,
                                    GVariant* parameters,
                                    GDBusMethodInvocation* invocation,
                                    gpointer user_data) {
        MyWindow* self = static_cast<MyWindow*>(user_data);

        std::cout << "收到DBus方法调用: " << method_name << std::endl;

        if (g_strcmp0(method_name, "Enable") == 0) {
            self->handle_enable_method(connection, invocation, parameters);
        } else if (g_strcmp0(method_name, "ShowNotification") == 0) {
            self->handle_show_notification_method(connection, invocation, parameters);
        } else {
            g_dbus_method_invocation_return_error(
                invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                "未知方法: %s", method_name);
        }
    }

    void handle_enable_method(GDBusConnection* connection,
                                GDBusMethodInvocation* invocation,
                                GVariant* parameters) {
        gboolean show_window = FALSE;
        gchar* message = nullptr;

        // 解析参数
        g_variant_get(parameters, "(bs)", &show_window, &message);

        // 转换为std::string
        std::string message_str;
        if (message) {
            // 检查字符串是否是有效的UTF-8
            if (g_utf8_validate(message, -1, nullptr)) {
                message_str = message;
                std::cout << "收到启用请求: show_window=" << show_window
                            << ", message=" << message_str << std::endl;
            } else {
                // 如果不是有效UTF-8，使用默认消息
                message_str = "从DBus启用";
                std::cerr << "警告: 接收到的消息不是有效的UTF-8字符串，使用默认消息。" << std::endl;
            }
            g_free(message);
        } else {
            message_str = "从DBus启用";
        }

        // 在主线程中执行启用操作
        Glib::signal_idle().connect_once([this, show_window, message_str]() {
            this->enable_from_dbus(show_window, message_str);
        });

        // 立即返回成功
        GVariant* result = g_variant_new("(b)", TRUE);
        g_dbus_method_invocation_return_value(invocation, result);
    }

    void handle_show_notification_method(GDBusConnection* connection,
                                            GDBusMethodInvocation* invocation,
                                            GVariant* parameters) {
        gchar* title = nullptr;
        gchar* message = nullptr;

        // 解析参数
        g_variant_get(parameters, "(ss)", &title, &message);

        // 转换为std::string
        std::string title_str, message_str;
        if (title && g_utf8_validate(title, -1, nullptr)) {
            title_str = title;
        } else {
            title_str = "Island2LocalSend";
        }

        if (message && g_utf8_validate(message, -1, nullptr)) {
            message_str = message;
        } else {
            message_str = "通知";
        }

        std::cout << "收到通知请求: title=" << title_str
                    << ", message=" << message_str << std::endl;

        if (title) g_free(title);
        if (message) g_free(message);

        // 发送状态变更信号
        send_status_changed_signal(is_enabled,
            std::string("收到通知: ") + (message_str));


        // 在主线程中执行通知显示
        Glib::signal_idle().connect_once([this, title_str, message_str]() {
            this->show_notification_from_dbus(title_str, message_str);
        });

        // 返回成功
        GVariant* result = g_variant_new("(b)", TRUE);
        g_dbus_method_invocation_return_value(invocation, result);
    }

    void enable_from_dbus(bool show_window, const std::string& message) {
        if (!is_enabled) {
            is_enabled = true;
            if (show_window) {
                start_auto_disable_timer();
                show();
            }
            update_app_indicator_icon();

            std::cout << "从DBus启用: " << message << std::endl;

            // 发送状态变更信号
            send_status_changed_signal(true, "从DBus启用: " + message);

            // 显示通知（可选）
            // show_notification_from_dbus("Island2LocalSend", message);
        }
    }

    void show_notification_from_dbus(const std::string& title, const std::string& message) {
        // 使用系统命令发送通知，确保字符串被正确引用
        std::string safe_title = escape_for_shell(title);
        std::string safe_message = escape_for_shell(message);

        std::string cmd = "notify-send \"" + safe_title + "\" \"" + safe_message + "\"";
        std::cout << "执行命令: " << cmd << std::endl;
        int result = std::system(cmd.c_str());

        if (result != 0) {
            std::cerr << "发送通知失败，退出码: " << result << std::endl;
        }
    }

    std::string escape_for_shell(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            if (c == '"' || c == '$' || c == '`' || c == '\\') {
                escaped.push_back('\\');
            }
            escaped.push_back(c);
        }
        return escaped;
    }

    void send_status_changed_signal(bool enabled, const std::string& status_message) {
        if (!dbus_connection) return;

        // 确保字符串是有效的UTF-8
        const gchar* msg = status_message.c_str();
        if (!g_utf8_validate(msg, -1, nullptr)) {
            msg = "状态已改变";
        }

        GVariant* signal_value = g_variant_new("(bs)", enabled, status_message.c_str());
        GError* error = nullptr;

        g_dbus_connection_emit_signal(
            dbus_connection,
            nullptr,  // 发送者，null表示使用连接的唯一名称
            "/com/kechen/island2localsend",
            "com.kechen.island2localsend",
            "StatusChanged",
            signal_value,
            &error);

        if (error) {
            std::cerr << "发送状态变更信号失败: " << error->message << std::endl;
            g_error_free(error);
        }
    }

    void start_auto_disable_timer() {
        // 取消现有的计时器
        cancel_auto_disable_timer();

        // 设置10秒后自动禁用
        auto_disable_conn = Glib::signal_timeout().connect([this]() {
            if (is_enabled && is_visible()) {
                std::cout << "鼠标离开10秒，自动禁用窗口" << std::endl;
                is_enabled = false;
                hide();
                send_status_changed_signal(false,"自动禁用");
                update_app_indicator_icon();
            }
            return false; // 只执行一次
        }, 10000); // 10秒 = 10000毫秒
    }

    void cancel_auto_disable_timer() {
        if (auto_disable_conn.connected()) {
            auto_disable_conn.disconnect();
        }
    }

    void create_app_indicator() {
        // 创建菜单
        Gtk::Menu* menu = Gtk::manage(new Gtk::Menu());

        // "启用"菜单项
        Gtk::MenuItem* enable_item = Gtk::manage(new Gtk::MenuItem("启用"));
        enable_item->signal_activate().connect([this]() {
            if (!is_enabled) {
                is_enabled = true;
                show(); // 显示窗口
                start_auto_disable_timer(); // 启动自动禁用计时器
                update_app_indicator_icon();
                send_status_changed_signal(true,"手动启用");
                std::cout << "已启用" << std::endl;
            }
        });
        menu->append(*enable_item);

        // "禁用"菜单项
        Gtk::MenuItem* disable_item = Gtk::manage(new Gtk::MenuItem("禁用"));
        disable_item->signal_activate().connect([this]() {
            if (is_enabled) {
                is_enabled = false;
                hide(); // 隐藏窗口
                update_app_indicator_icon();
                cancel_auto_disable_timer();
                send_status_changed_signal(false,"手动禁用");
                std::cout << "已禁用" << std::endl;
            }
        });
        menu->append(*disable_item);

        // 分隔线
        Gtk::SeparatorMenuItem* separator = Gtk::manage(new Gtk::SeparatorMenuItem());
        menu->append(*separator);

        // "退出"菜单项
        Gtk::MenuItem* quit_item = Gtk::manage(new Gtk::MenuItem("退出"));
        quit_item->signal_activate().connect([this]() {
            std::cout << "退出程序" << std::endl;
            // 清理资源并退出
            if (app_indicator) {
                g_object_unref(app_indicator);
                app_indicator = nullptr;
            }
            // 退出主循环
            // Gtk::Main::quit();
            // 彻底退出程序，释放 hold
            if (auto app = Gtk::Application::get_default()) {
                app->quit();
            }
        });
        menu->append(*quit_item);

        menu->show_all();

        // 创建 AppIndicator - 使用 Ayatana 版本
        app_indicator = app_indicator_new(
            "island-sender-app", // 唯一ID
            "",         // 图标名称
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS
        );

        // 设置菜单
        app_indicator_set_menu(app_indicator, GTK_MENU(menu->gobj()));

        // 设置状态
        app_indicator_set_status(app_indicator, APP_INDICATOR_STATUS_ACTIVE);

        // 设置图标标题
        app_indicator_set_title(app_indicator, "LocalSend 岛屿发送器");
        update_app_indicator_icon();
    }
    void update_app_indicator_icon() {
        if (!app_indicator) return;

        if (is_enabled) {
            app_indicator_set_icon_full(
                app_indicator,
                "folder-publicshare",
                "LocalSend 发送器 (已启用)"
            );
        } else {
            app_indicator_set_icon_full(
                app_indicator,
                "folder-publicshare-symbolic",
                "LocalSend 发送器 (已禁用)"
            );
        }
    }


    void query_gsconnect_devices() {
        if (gsconnect_checked) return;
        gsconnect_checked = true;
        gsconnect_available = false;
        gsconnect_device = {};
        gsconnect_devices.clear();

        std::string cmd =
            "gdbus call --session "
            "--dest org.gnome.Shell.Extensions.GSConnect "
            "--object-path /org/gnome/Shell/Extensions/GSConnect "
            "--method org.freedesktop.DBus.ObjectManager.GetManagedObjects "
            "2>/dev/null";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return;

        std::string output;
        char buffer[8192];
        while (fgets(buffer, sizeof(buffer), pipe))
            output += buffer;
        pclose(pipe);

        std::regex device_re("/org/gnome/Shell/Extensions/GSConnect/Device/([^']+)");
        std::sregex_iterator it(output.begin(), output.end(), device_re);
        std::sregex_iterator end;

        for (; it != end; ++it) {
            std::string dev_id = (*it)[1];
            std::string dev_path = (*it)[0];

            size_t pos = output.find(dev_path);
            if (pos == std::string::npos) continue;

            std::string section = output.substr(pos, output.find("},", pos) - pos);
            std::regex name_re("'Name':\\s*<'([^']*)'");
            std::regex conn_re("'Connected':\\s*<([^>]+)>");
            std::smatch nm, cm;

            GSConnectDevice dev;
            dev.id = dev_id;
            if (std::regex_search(section, nm, name_re))
                dev.name = nm[1];
            if (std::regex_search(section, cm, conn_re))
                dev.connected = (cm[1] == "true");

            gsconnect_devices.push_back(dev);
            std::cout << "[GSConnect] 设备: " << dev.name
                      << " (" << dev.id << ") "
                      << (dev.connected ? "在线" : "离线") << std::endl;
        }

        if (!gsconnect_devices.empty()) {
            gsconnect_available = true;
            for (auto& d : gsconnect_devices) {
                if (d.connected) {
                    gsconnect_device = d;
                    break;
                }
            }
            if (gsconnect_device.id.empty())
                gsconnect_device = gsconnect_devices[0];
        }
    }

    void send_via_gsconnect(const std::vector<std::string>& paths) {
        if (!gsconnect_available || gsconnect_device.id.empty()) return;
        gsconnect_checked = false;
        std::string dev_id = gsconnect_device.id;

        for (const auto& path : paths) {
            std::string cmd =
                "gdbus call --session"
                " --dest org.gnome.Shell.Extensions.GSConnect"
                " --object-path /org/gnome/Shell/Extensions/GSConnect/Device/" + dev_id +
                " --method org.gtk.Actions.Activate"
                " shareFile"
                " \"[<('" + escape_for_shell(path) + "', false)>]\""
                " {}"
                " 2>&1 &";
            std::cout << "[GSConnect] " << cmd << std::endl;
            std::system(cmd.c_str());
        }
    }

    bool on_drawing_area_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
        int win_w = drawing_area.get_allocated_width();
        int win_h = drawing_area.get_allocated_height();

        // A. 彻底清空物理窗口背景（全透明）
        cr->save();
        cr->set_operator(Cairo::OPERATOR_SOURCE);
        cr->set_source_rgba(0, 0, 0, 0);
        cr->paint();
        cr->restore();

        if (current_h < 1.0) return true;

        // B. 调试边框（感应区范围）- 2026 调试完请删除
        // cr->set_source_rgba(1, 0, 0, 0.2);
        // cr->set_line_width(1);
        // cr->rectangle(0, 0, win_w, win_h);
        // cr->stroke();

        // C. 计算内部岛屿的居中起始坐标
        // double island_x = (win_w - current_w) / 2.0;
        // double island_y = 0; // 紧贴顶部绘制
        // 2. 计算胶囊位置
        double island_x = (win_w - current_w) / 2.0;
        double island_y = current_y; // 这里的 current_y 会随动画从负值变正
        double radius = current_h / 2.0; // 胶囊样式：圆角半径始终是高度的一半

        cr->set_antialias(Cairo::ANTIALIAS_DEFAULT);

        // D. 绘制黑色岛屿背景
        auto pill_path = [&]() {
            cr->begin_new_path();
            cr->arc(island_x + current_w - radius, island_y + radius, radius, -M_PI/2, 0);
            cr->arc(island_x + current_w - radius, island_y + current_h - radius, radius, 0, M_PI/2);
            cr->arc(island_x + radius, island_y + current_h - radius, radius, M_PI/2, M_PI);
            cr->arc(island_x + radius, island_y + radius, radius, M_PI, 3*M_PI/2);
            cr->close_path();
        };

        pill_path();
        cr->set_source_rgba(0.05, 0.05, 0.05, 1.0);
        cr->fill();

        // E. 渐变色填充
        if (std::abs(current_fill) > 0.005) {
            double pad = fill_padding;
            double fill_r = (current_h - pad * 2) / 2.0;
            double full_w = current_w - pad * 2;
            double abs_fill = std::abs(current_fill);
            double fill_w = abs_fill * full_w;
            double inner_r = std::min(fill_r, fill_w / 2.0);
            bool use_arc = (fill_w >= fill_r * 2.0);

            cr->save();
            cr->begin_new_path();
            cr->arc(island_x + current_w - pad - fill_r, island_y + pad + fill_r, fill_r, -M_PI/2, 0);
            cr->arc(island_x + current_w - pad - fill_r, island_y + current_h - pad - fill_r, fill_r, 0, M_PI/2);
            cr->arc(island_x + pad + fill_r, island_y + current_h - pad - fill_r, fill_r, M_PI/2, M_PI);
            cr->arc(island_x + pad + fill_r, island_y + pad + fill_r, fill_r, M_PI, 3*M_PI/2);
            cr->close_path();
            cr->clip();

            double cy = island_y + current_h / 2.0;
            if (current_fill < 0) {
                if (use_arc) {
                    double tip = island_x + pad + fill_w;
                    cr->move_to(tip, island_y + pad);
                    cr->arc(tip, cy, inner_r, -M_PI / 2, M_PI / 2);
                    cr->line_to(island_x + pad, island_y + current_h - pad);
                    cr->line_to(island_x + pad, island_y + pad);
                    cr->close_path();
                } else {
                    cr->rectangle(island_x + pad, island_y + pad, fill_w, current_h - pad * 2);
                }
                cr->set_source_rgba(0.0, 0.478, 1.0, 0.55);
            } else {
                if (use_arc) {
                    double tip = island_x + current_w - pad - fill_w;
                    cr->move_to(tip, island_y + pad);
                    cr->arc_negative(tip, cy, inner_r, 3 * M_PI / 2, M_PI / 2);
                    cr->line_to(island_x + current_w - pad, island_y + current_h - pad);
                    cr->line_to(island_x + current_w - pad, island_y + pad);
                    cr->close_path();
                } else {
                    cr->rectangle(island_x + current_w - pad - fill_w, island_y + pad, fill_w, current_h - pad * 2);
                }
                cr->set_source_rgba(0.204, 0.780, 0.349, 0.55);
            }
            cr->fill();
            cr->restore();
        }

        // F. 设备列表
        if (show_device_list && !gsconnect_devices.empty()) {
            int total = (int)gsconnect_devices.size();
            double pad = fill_padding;
            int vis = std::min(total, 3);
            double inner_gap = (vis == 1) ? 0.0 : 6.0;
            double btn_w = (current_w - pad * 2 - inner_gap * (vis - 1)) / vis;
            double btn_h = current_h - pad * 2;
            double btn_y = island_y + pad;
            double cr_r = 5.0;
            double step = btn_w + inner_gap;

            cr->save();
            double ir = (current_h - pad * 2) / 2.0;
            cr->begin_new_path();
            cr->arc(island_x + current_w - pad - ir, island_y + pad + ir, ir, -M_PI/2, 0);
            cr->arc(island_x + current_w - pad - ir, island_y + current_h - pad - ir, ir, 0, M_PI/2);
            cr->arc(island_x + pad + ir, island_y + current_h - pad - ir, ir, M_PI/2, M_PI);
            cr->arc(island_x + pad + ir, island_y + pad + ir, ir, M_PI, 3*M_PI/2);
            cr->close_path();
            cr->clip();

            for (int i = 0; i < total; i++) {
                double bx = island_x + pad + (i - scroll_offset) * step;
                if (bx + btn_w < island_x + pad || bx > island_x + current_w - pad) continue;
                bool hovered = (i == gsconnect_hovered);
                bool online = gsconnect_devices[i].connected;

                cr->begin_new_path();
                cr->arc(bx + btn_w - cr_r, btn_y + cr_r, cr_r, -M_PI/2, 0);
                cr->arc(bx + btn_w - cr_r, btn_y + btn_h - cr_r, cr_r, 0, M_PI/2);
                cr->arc(bx + cr_r, btn_y + btn_h - cr_r, cr_r, M_PI/2, M_PI);
                cr->arc(bx + cr_r, btn_y + cr_r, cr_r, M_PI, 3*M_PI/2);
                cr->close_path();

                if (online) {
                    if (hovered)
                        cr->set_source_rgba(0.15, 0.65, 0.30, 0.55);
                    else
                        cr->set_source_rgba(0.10, 0.55, 0.25, 0.20);
                } else {
                    if (hovered)
                        cr->set_source_rgba(1, 1, 1, 0.08);
                    else
                        cr->set_source_rgba(1, 1, 1, 0.02);
                }
                cr->fill();

                double vis_l = std::max(bx, island_x + pad);
                double vis_r = std::min(bx + btn_w, island_x + current_w - pad);
                double vis_cx = (vis_l + vis_r) / 2.0;

                std::string name = gsconnect_devices[i].name;
                auto layout = create_pango_layout("");
                layout->set_font_description(Pango::FontDescription("Sans Bold 9"));
                layout->set_text(name);
                int lw = (int)(btn_w * 0.85);
                layout->set_width(lw * PANGO_SCALE);
                layout->set_wrap(Pango::WRAP_WORD_CHAR);
                layout->set_alignment(Pango::ALIGN_CENTER);

                double dot_r = 4.5;
                double dot_y = btn_y + btn_h * 0.38;

                cr->set_source_rgba(
                    online ? 0.204 : 0.45,
                    online ? 0.780 : 0.45,
                    online ? 0.349 : 0.45,
                    online ? 0.95 : 0.5);
                cr->arc(vis_cx, dot_y, dot_r, 0, 2 * M_PI);
                cr->fill();

                cr->set_source_rgba(1, 1, 1, online ? 0.95 : 0.5);
                cr->move_to(vis_cx - lw / 2.0, dot_y + 10);
                pango_cairo_show_layout(cr->cobj(), layout->gobj());
            }

            cr->restore();

            if (total > 3) {
                if (scroll_offset > 0.01 || scroll_offset < scroll_max - 0.01) {
                    double ar = radius + 3.0;
                    double ay = island_y + radius;
                    double seg = M_PI / 12;
                    cr->set_source_rgba(0.05, 0.05, 0.05, 1.0);
                    cr->set_line_width(3.5);
                    cr->set_line_cap(Cairo::LINE_CAP_ROUND);
                    if (scroll_offset > 0.01) {
                        cr->begin_new_path();
                        cr->arc(island_x + radius, ay, ar, M_PI - seg, M_PI + seg);
                        cr->stroke();
                    }
                    if (scroll_offset < scroll_max - 0.01) {
                        cr->begin_new_path();
                        cr->arc(island_x + current_w - radius, ay, ar, -seg, seg);
                        cr->stroke();
                    }
                }
            }
        }

        // 岛屿边框 (最上层)
        pill_path();
        cr->set_source_rgba(1, 1, 1, 0.15);
        cr->set_line_width(1.2);
        cr->stroke();

        return true;
    }

    void start_animation() {
        if (!tick_conn.connected()) {
            tick_conn = Glib::signal_timeout().connect([this]() {
                bounce_vel = (bounce_vel + (-bounce_pos) * 0.15) * 0.75;
                bounce_pos += bounce_vel;
                double bscale = 1.0 + 0.10 * bounce_pos;
                vel_h = (vel_h + (target_h * bscale - current_h) * stiffness) * damping;
                current_h += vel_h;
                vel_w = (vel_w + (target_w * bscale - current_w) * stiffness) * damping;
                current_w += vel_w;
                vel_y = (vel_y + (target_y - current_y) * stiffness) * damping;
                current_y += vel_y;
                vel_fill = (vel_fill + (target_fill - current_fill) * stiffness) * damping;
                current_fill += vel_fill;
                scroll_vel = (scroll_vel + (target_scroll - scroll_offset) * 0.25) * 0.78;
                target_scroll += scroll_rate;
                target_scroll = std::clamp(target_scroll, 0.0, scroll_max);
                double next_so = scroll_offset + scroll_vel;
                if ((next_so < 0.0 && scroll_offset > 0.0) ||
                    (next_so > scroll_max && scroll_offset < scroll_max - 0.01))
                    bounce_vel = std::max(bounce_vel, 0.12);
                scroll_offset = std::clamp(next_so, 0.0, scroll_max);

                int label_height = label.get_allocated_height() > 0 ? label.get_allocated_height() : 20;

                int top_margin = (int)(current_y + (current_h - label_height) / 2.0);
                label.set_margin_top(std::max(0, top_margin));
                double alpha = (current_h - 40.0) / 30.0;
                label.set_opacity(std::clamp(alpha, 0.0, 1.0));


                queue_draw(); // 仅重绘，不改变物理窗口大小

                if (std::abs(target_h - current_h) < 0.05 && std::abs(vel_h) < 0.05 &&
                    std::abs(target_w - current_w) < 0.05 && std::abs(vel_w) < 0.05 &&
                    std::abs(target_y - current_y) < 0.05 && std::abs(vel_y) < 0.05 &&
                    std::abs(target_fill - current_fill) < 0.005 && std::abs(vel_fill) < 0.005 &&
                    std::abs(target_scroll - scroll_offset) < 0.005 && std::abs(scroll_vel) < 0.005 &&
                    std::abs(bounce_pos) < 0.005 && std::abs(bounce_vel) < 0.005) {
                    current_h = target_h; current_w = target_w; current_y = target_y;
                    current_fill = target_fill; scroll_offset = target_scroll;
                    bounce_pos = 0.0; bounce_vel = 0.0;
                    return false;
                }
                return true;
            }, 16);
        }
    }
};

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "com.kechen.island2localsend");
    MyWindow win;
    return app->run(win);
}
