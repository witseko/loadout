#include "surfaces/webview.hpp"

#include <dlfcn.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace loadout::surfaces {

namespace {

template <typename Fn>
bool load_sym(void* handle, const char* name, Fn& out) {
    void* sym = ::dlsym(handle, name);
    if (!sym) {
        return false;
    }
    static_assert(sizeof(Fn) == sizeof(void*));
    std::memcpy(&out, &sym, sizeof(Fn));
    return true;
}

} // namespace

#if defined(__APPLE__) && !defined(LOADOUT_FORCE_LINUX_WEBVIEW)

namespace {

using id = void*;
using Class = void*;
using SEL = void*;
using ObjcBool = signed char;

struct NSPoint {
    double x;
    double y;
};

struct NSSize {
    double width;
    double height;
};

struct NSRect {
    NSPoint origin;
    NSSize size;
};

using ObjcGetClassFn = Class (*)(const char*);
using SelRegisterNameFn = SEL (*)(const char*);
using ObjcAllocateClassPairFn = Class (*)(Class, const char*, size_t);
using ObjcRegisterClassPairFn = void (*)(Class);
using ClassAddMethodFn = ObjcBool (*)(Class, SEL, void*, const char*);

struct MacRuntime {
    void* libobjc = nullptr;
    void* foundation = nullptr;
    void* cocoa = nullptr;
    void* webkit = nullptr;

    ObjcGetClassFn get_class = nullptr;
    SelRegisterNameFn sel_register = nullptr;
    ObjcAllocateClassPairFn alloc_class_pair = nullptr;
    ObjcRegisterClassPairFn register_class_pair = nullptr;
    ClassAddMethodFn add_method = nullptr;
    void* msg_send_sym = nullptr;

    template <typename Ret, typename... Args>
    Ret send(id obj, SEL sel, Args... args) const {
        using Fn = Ret (*)(id, SEL, Args...);
        Fn fn = nullptr;
        std::memcpy(&fn, &msg_send_sym, sizeof(fn));
        return fn(obj, sel, args...);
    }

    SEL sel(const char* name) const {
        return sel_register(name);
    }

    Class cls(const char* name) const {
        return get_class(name);
    }

    id nsstring(const char* s) const {
        return send<id, const char*>(cls("NSString"), sel("stringWithUTF8String:"), s);
    }

    bool load() {
        libobjc = ::dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY);
        foundation = ::dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY);
        cocoa = ::dlopen("/System/Library/Frameworks/Cocoa.framework/Cocoa", RTLD_LAZY);
        webkit = ::dlopen("/System/Library/Frameworks/WebKit.framework/WebKit", RTLD_LAZY);
        if (!libobjc || !foundation || !cocoa || !webkit) {
            return false;
        }
        msg_send_sym = ::dlsym(libobjc, "objc_msgSend");
        if (!msg_send_sym) {
            return false;
        }
        return load_sym(libobjc, "objc_getClass", get_class) &&
               load_sym(libobjc, "sel_registerName", sel_register) &&
               load_sym(libobjc, "objc_allocateClassPair", alloc_class_pair) &&
               load_sym(libobjc, "objc_registerClassPair", register_class_pair) &&
               load_sym(libobjc, "class_addMethod", add_method);
    }
};

std::mutex g_mac_mutex;
MacRuntime g_mac_rt;
id g_mac_app = nullptr;
id g_mac_delegate = nullptr;
bool g_mac_running = false;

void stop_mac_runloop_on_main(const MacRuntime& rt, id app) {
    if (!app) {
        return;
    }
    rt.send<void, id>(app, rt.sel("stop:"), nullptr);
    // Post a synthetic application-defined event so -[NSApplication run] wakes up immediately.
    id ev = rt.send<id, uint64_t, NSPoint, uint64_t, double, int64_t, id, int16_t, int64_t, int64_t>(
        rt.cls("NSEvent"),
        rt.sel("otherEventWithType:location:modifierFlags:timestamp:windowNumber:context:subtype:data1:data2:"),
        15ULL, // NSEventTypeApplicationDefined
        NSPoint{0.0, 0.0},
        0ULL,
        0.0,
        0LL,
        nullptr,
        static_cast<int16_t>(0),
        0LL,
        0LL);
    if (ev) {
        rt.send<void, id, ObjcBool>(app, rt.sel("postEvent:atStart:"), ev, static_cast<ObjcBool>(1));
    }
}

void mac_window_will_close(id /*self*/, SEL /*cmd*/, id /*notification*/) {
    std::lock_guard<std::mutex> lock(g_mac_mutex);
    if (g_mac_running && g_mac_app) {
        stop_mac_runloop_on_main(g_mac_rt, g_mac_app);
    }
}

uint64_t mac_app_should_terminate(id /*self*/, SEL /*cmd*/, id /*sender*/) {
    std::lock_guard<std::mutex> lock(g_mac_mutex);
    if (g_mac_running && g_mac_app) {
        stop_mac_runloop_on_main(g_mac_rt, g_mac_app);
    }
    return 0ULL; // NSTerminateCancel: return cleanly from -[NSApplication run] instead of exit(0)
}

void mac_stop_from_signal(id /*self*/, SEL /*cmd*/, id /*sender*/) {
    std::lock_guard<std::mutex> lock(g_mac_mutex);
    if (g_mac_running && g_mac_app) {
        stop_mac_runloop_on_main(g_mac_rt, g_mac_app);
    }
}

id make_menu_item(const MacRuntime& rt, const char* title, const char* action, const char* key,
                  id target = nullptr, uint64_t modifier_mask = 0ULL) {
    id alloc_item = rt.send<id>(rt.cls("NSMenuItem"), rt.sel("alloc"));
    SEL action_sel = (action && action[0] != '\0') ? rt.sel(action) : nullptr;
    id item = rt.send<id, id, SEL, id>(
        alloc_item,
        rt.sel("initWithTitle:action:keyEquivalent:"),
        rt.nsstring(title),
        action_sel,
        rt.nsstring(key));
    if (target) {
        rt.send<void, id>(item, rt.sel("setTarget:"), target);
    }
    if (modifier_mask != 0ULL) {
        rt.send<void, uint64_t>(item, rt.sel("setKeyEquivalentModifierMask:"), modifier_mask);
    }
    return item;
}

id make_submenu(const MacRuntime& rt, const char* title) {
    id alloc_menu = rt.send<id>(rt.cls("NSMenu"), rt.sel("alloc"));
    return rt.send<id, id>(alloc_menu, rt.sel("initWithTitle:"), rt.nsstring(title));
}

void add_separator(const MacRuntime& rt, id menu) {
    id sep = rt.send<id>(rt.cls("NSMenuItem"), rt.sel("separatorItem"));
    rt.send<void, id>(menu, rt.sel("addItem:"), sep);
}

void install_mac_menubar(const MacRuntime& rt, id app, id webview) {
    constexpr uint64_t kCmdMask = 1ULL << 20;
    constexpr uint64_t kOptMask = 1ULL << 19;
    constexpr uint64_t kCtrlMask = 1ULL << 18;

    id menubar = make_submenu(rt, "");

    // 1. Application menu
    id app_menu_item = make_menu_item(rt, "", "", "");
    id app_menu = make_submenu(rt, "loadout");
    rt.send<void, id>(app_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "About loadout", "orderFrontStandardAboutPanel:", ""));
    add_separator(rt, app_menu);
    rt.send<void, id>(app_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Hide loadout", "hide:", "h"));
    rt.send<void, id>(app_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Hide Others", "hideOtherApplications:", "h", nullptr, kCmdMask | kOptMask));
    rt.send<void, id>(app_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Show All", "unhideAllApplications:", ""));
    add_separator(rt, app_menu);
    rt.send<void, id>(app_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Quit loadout", "terminate:", "q"));
    rt.send<void, id>(app_menu_item, rt.sel("setSubmenu:"), app_menu);
    rt.send<void, id>(menubar, rt.sel("addItem:"), app_menu_item);

    // 2. Edit menu (enables Cmd+C/V/X/A/Z inside WKWebView)
    id edit_menu_item = make_menu_item(rt, "Edit", "", "");
    id edit_menu = make_submenu(rt, "Edit");
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Undo", "undo:", "z"));
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Redo", "redo:", "Z"));
    add_separator(rt, edit_menu);
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Cut", "cut:", "x"));
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Copy", "copy:", "c"));
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Paste", "paste:", "v"));
    rt.send<void, id>(edit_menu, rt.sel("addItem:"), make_menu_item(rt, "Select All", "selectAll:", "a"));
    rt.send<void, id>(edit_menu_item, rt.sel("setSubmenu:"), edit_menu);
    rt.send<void, id>(menubar, rt.sel("addItem:"), edit_menu_item);

    // 3. View menu
    id view_menu_item = make_menu_item(rt, "View", "", "");
    id view_menu = make_submenu(rt, "View");
    rt.send<void, id>(view_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Reload", "reload:", "r", webview));
    rt.send<void, id>(view_menu, rt.sel("addItem:"),
                      make_menu_item(rt, "Toggle Full Screen", "toggleFullScreen:", "f", nullptr, kCmdMask | kCtrlMask));
    rt.send<void, id>(view_menu_item, rt.sel("setSubmenu:"), view_menu);
    rt.send<void, id>(menubar, rt.sel("addItem:"), view_menu_item);

    // 4. Window menu
    id win_menu_item = make_menu_item(rt, "Window", "", "");
    id win_menu = make_submenu(rt, "Window");
    rt.send<void, id>(win_menu, rt.sel("addItem:"), make_menu_item(rt, "Minimize", "performMiniaturize:", "m"));
    rt.send<void, id>(win_menu, rt.sel("addItem:"), make_menu_item(rt, "Zoom", "performZoom:", ""));
    add_separator(rt, win_menu);
    rt.send<void, id>(win_menu, rt.sel("addItem:"), make_menu_item(rt, "Close Window", "performClose:", "w"));
    rt.send<void, id>(win_menu_item, rt.sel("setSubmenu:"), win_menu);
    rt.send<void, id>(menubar, rt.sel("addItem:"), win_menu_item);

    rt.send<void, id>(app, rt.sel("setMainMenu:"), menubar);
}

Class get_or_register_delegate_class(const MacRuntime& rt) {
    Class existing = rt.cls("LoadoutNativeAppDelegate");
    if (existing) {
        return existing;
    }
    Class ns_object = rt.cls("NSObject");
    Class delegate_cls = rt.alloc_class_pair(ns_object, "LoadoutNativeAppDelegate", 0);
    if (!delegate_cls) {
        return nullptr;
    }

    void* close_imp = nullptr;
    auto close_fn = &mac_window_will_close;
    std::memcpy(&close_imp, &close_fn, sizeof(close_imp));
    rt.add_method(delegate_cls, rt.sel("windowWillClose:"), close_imp, "v@:@");

    void* term_imp = nullptr;
    auto term_fn = &mac_app_should_terminate;
    std::memcpy(&term_imp, &term_fn, sizeof(term_imp));
    rt.add_method(delegate_cls, rt.sel("applicationShouldTerminate:"), term_imp, "Q@:@");

    void* stop_imp = nullptr;
    auto stop_fn = &mac_stop_from_signal;
    std::memcpy(&stop_imp, &stop_fn, sizeof(stop_imp));
    rt.add_method(delegate_cls, rt.sel("loadoutStopRunLoop:"), stop_imp, "v@:@");

    rt.register_class_pair(delegate_cls);
    return delegate_cls;
}

} // namespace

NativeWindowResult run_native_window(const NativeWindowOptions& opts) {
    MacRuntime rt;
    if (!rt.load()) {
        return NativeWindowResult::Unavailable;
    }

    id pool = rt.send<id>(rt.send<id>(rt.cls("NSAutoreleasePool"), rt.sel("alloc")), rt.sel("init"));

    id app = rt.send<id>(rt.cls("NSApplication"), rt.sel("sharedApplication"));
    if (!app) {
        if (pool) rt.send<void>(pool, rt.sel("drain"));
        return NativeWindowResult::Unavailable;
    }

    // NSApplicationActivationPolicyRegular (0): show in Dock + native menu bar
    rt.send<ObjcBool, int64_t>(app, rt.sel("setActivationPolicy:"), 0LL);

    Class delegate_cls = get_or_register_delegate_class(rt);
    if (!delegate_cls) {
        if (pool) rt.send<void>(pool, rt.sel("drain"));
        return NativeWindowResult::Failed;
    }
    id delegate = rt.send<id>(rt.send<id>(delegate_cls, rt.sel("alloc")), rt.sel("init"));
    rt.send<void, id>(app, rt.sel("setDelegate:"), delegate);

    // Style mask: Titled (1) | Closable (2) | Miniaturizable (4) | Resizable (8)
    constexpr uint64_t kStyleMask = 1ULL | 2ULL | 4ULL | 8ULL;
    constexpr uint64_t kBackingBuffered = 2ULL;

    NSRect frame{{0.0, 0.0}, {static_cast<double>(opts.width), static_cast<double>(opts.height)}};
    id window = rt.send<id, NSRect, uint64_t, uint64_t, ObjcBool>(
        rt.send<id>(rt.cls("NSWindow"), rt.sel("alloc")),
        rt.sel("initWithContentRect:styleMask:backing:defer:"),
        frame,
        kStyleMask,
        kBackingBuffered,
        static_cast<ObjcBool>(0));
    if (!window) {
        if (pool) rt.send<void>(pool, rt.sel("drain"));
        return NativeWindowResult::Failed;
    }

    rt.send<void, id>(window, rt.sel("setTitle:"), rt.nsstring(opts.title.c_str()));
    rt.send<void, NSSize>(
        window,
        rt.sel("setMinSize:"),
        NSSize{static_cast<double>(opts.min_width), static_cast<double>(opts.min_height)});
    rt.send<void, id>(window, rt.sel("setDelegate:"), delegate);

    if (opts.dark_titlebar) {
        // Match loadout's #0B0D10 dark surface to prevent any white flash on startup
        id bg = rt.send<id, double, double, double, double>(
            rt.cls("NSColor"),
            rt.sel("colorWithSRGBRed:green:blue:alpha:"),
            11.0 / 255.0,
            13.0 / 255.0,
            16.0 / 255.0,
            1.0);
        if (bg) {
            rt.send<void, id>(window, rt.sel("setBackgroundColor:"), bg);
        }
        id dark_appearance = rt.send<id, id>(
            rt.cls("NSAppearance"),
            rt.sel("appearanceNamed:"),
            rt.nsstring("NSAppearanceNameDarkAqua"));
        if (dark_appearance) {
            rt.send<void, id>(window, rt.sel("setAppearance:"), dark_appearance);
        }
    }

    // Create WKWebViewConfiguration & WKWebView
    id cfg = rt.send<id>(rt.send<id>(rt.cls("WKWebViewConfiguration"), rt.sel("alloc")), rt.sel("init"));
    id prefs = rt.send<id>(cfg, rt.sel("preferences"));
    if (prefs) {
        id yes_num = rt.send<id, ObjcBool>(rt.cls("NSNumber"), rt.sel("numberWithBool:"), static_cast<ObjcBool>(1));
        rt.send<void, id, id>(prefs, rt.sel("setValue:forKey:"), yes_num, rt.nsstring("developerExtrasEnabled"));
    }

    id webview = rt.send<id, NSRect, id>(
        rt.send<id>(rt.cls("WKWebView"), rt.sel("alloc")),
        rt.sel("initWithFrame:configuration:"),
        frame,
        cfg);
    if (!webview) {
        if (pool) rt.send<void>(pool, rt.sel("drain"));
        return NativeWindowResult::Failed;
    }

    // Transparent WebView backdrop so the dark NSWindow background shows during initial paint
    id no_num = rt.send<id, ObjcBool>(rt.cls("NSNumber"), rt.sel("numberWithBool:"), static_cast<ObjcBool>(0));
    rt.send<void, id, id>(webview, rt.sel("setValue:forKey:"), no_num, rt.nsstring("drawsBackground"));

    // NSViewWidthSizable (2) | NSViewHeightSizable (16)
    rt.send<void, uint64_t>(webview, rt.sel("setAutoresizingMask:"), 18ULL);
    rt.send<void, id>(window, rt.sel("setContentView:"), webview);

    install_mac_menubar(rt, app, webview);

    // Load local loadout URL
    id nsurl = rt.send<id, id>(rt.cls("NSURL"), rt.sel("URLWithString:"), rt.nsstring(opts.url.c_str()));
    id req = rt.send<id, id>(rt.cls("NSURLRequest"), rt.sel("requestWithURL:"), nsurl);
    rt.send<id, id>(webview, rt.sel("loadRequest:"), req);

    rt.send<void>(window, rt.sel("center"));
    rt.send<void, id>(window, rt.sel("makeKeyAndOrderFront:"), nullptr);
    rt.send<void, ObjcBool>(app, rt.sel("activateIgnoringOtherApps:"), static_cast<ObjcBool>(1));

    {
        std::lock_guard<std::mutex> lock(g_mac_mutex);
        g_mac_rt = rt;
        g_mac_app = app;
        g_mac_delegate = delegate;
        g_mac_running = true;
    }

    rt.send<void>(app, rt.sel("run"));

    {
        std::lock_guard<std::mutex> lock(g_mac_mutex);
        g_mac_running = false;
        g_mac_app = nullptr;
        g_mac_delegate = nullptr;
    }

    rt.send<void, id>(window, rt.sel("setDelegate:"), nullptr);
    rt.send<void, id>(app, rt.sel("setDelegate:"), nullptr);
    rt.send<void, id>(window, rt.sel("orderOut:"), nullptr);
    if (pool) {
        rt.send<void>(pool, rt.sel("drain"));
    }

    return NativeWindowResult::Closed;
}

void request_native_window_close() {
    std::lock_guard<std::mutex> lock(g_mac_mutex);
    if (!g_mac_running || !g_mac_delegate) {
        return;
    }
    g_mac_rt.send<void, SEL, id, ObjcBool>(
        g_mac_delegate,
        g_mac_rt.sel("performSelectorOnMainThread:withObject:waitUntilDone:"),
        g_mac_rt.sel("loadoutStopRunLoop:"),
        nullptr,
        static_cast<ObjcBool>(0));
}

#else // Linux / POSIX (GTK3 + WebKitGTK dynamically loaded via dlopen)

namespace {

using GtkWidget = void;
using GtkWindow = void;
using GtkContainer = void;
using GtkSettings = void;
using WebKitWebView = void;
using GCallback = void (*)(void);
using GSourceFunc = int (*)(void*);

struct GdkRGBA {
    double red;
    double green;
    double blue;
    double alpha;
};

using GtkInitCheckFn = int (*)(int*, char***);
using GtkWindowNewFn = GtkWidget* (*)(int);
using GtkWindowSetTitleFn = void (*)(GtkWindow*, const char*);
using GtkWindowSetDefaultSizeFn = void (*)(GtkWindow*, int, int);
using GtkWidgetSetSizeRequestFn = void (*)(GtkWidget*, int, int);
using GtkContainerAddFn = void (*)(GtkContainer*, GtkWidget*);
using GtkWidgetShowAllFn = void (*)(GtkWidget*);
using GtkMainFn = void (*)(void);
using GtkMainQuitFn = void (*)(void);
using GtkSettingsGetDefaultFn = GtkSettings* (*)(void);

using WebKitWebViewNewFn = GtkWidget* (*)(void);
using WebKitWebViewLoadUriFn = void (*)(WebKitWebView*, const char*);
using WebKitWebViewSetBgFn = void (*)(WebKitWebView*, const GdkRGBA*);

using GSignalConnectDataFn = unsigned long (*)(void*, const char*, GCallback, void*, void*, int);
using GObjectSetFn = void (*)(void*, const char*, ...);
using GIdleAddFn = unsigned int (*)(GSourceFunc, void*);

struct LinuxRuntime {
    void* gtk = nullptr;
    void* webkit = nullptr;
    void* gobject = nullptr;
    void* glib = nullptr;

    GtkInitCheckFn gtk_init_check = nullptr;
    GtkWindowNewFn gtk_window_new = nullptr;
    GtkWindowSetTitleFn gtk_window_set_title = nullptr;
    GtkWindowSetDefaultSizeFn gtk_window_set_default_size = nullptr;
    GtkWidgetSetSizeRequestFn gtk_widget_set_size_request = nullptr;
    GtkContainerAddFn gtk_container_add = nullptr;
    GtkWidgetShowAllFn gtk_widget_show_all = nullptr;
    GtkMainFn gtk_main = nullptr;
    GtkMainQuitFn gtk_main_quit = nullptr;
    GtkSettingsGetDefaultFn gtk_settings_get_default = nullptr;

    WebKitWebViewNewFn webkit_web_view_new = nullptr;
    WebKitWebViewLoadUriFn webkit_web_view_load_uri = nullptr;
    WebKitWebViewSetBgFn webkit_web_view_set_background_color = nullptr;

    GSignalConnectDataFn g_signal_connect_data = nullptr;
    GObjectSetFn g_object_set = nullptr;
    GIdleAddFn g_idle_add = nullptr;

    bool load() {
        gtk = ::dlopen("libgtk-3.so.0", RTLD_LAZY);
        if (!gtk) return false;

        webkit = ::dlopen("libwebkit2gtk-4.1.so.0", RTLD_LAZY);
        if (!webkit) {
            webkit = ::dlopen("libwebkit2gtk-4.0.so.37", RTLD_LAZY);
        }
        if (!webkit) return false;

        gobject = ::dlopen("libgobject-2.0.so.0", RTLD_LAZY);
        glib = ::dlopen("libglib-2.0.so.0", RTLD_LAZY);
        if (!gobject || !glib) return false;

        bool ok =
            load_sym(gtk, "gtk_init_check", gtk_init_check) &&
            load_sym(gtk, "gtk_window_new", gtk_window_new) &&
            load_sym(gtk, "gtk_window_set_title", gtk_window_set_title) &&
            load_sym(gtk, "gtk_window_set_default_size", gtk_window_set_default_size) &&
            load_sym(gtk, "gtk_widget_set_size_request", gtk_widget_set_size_request) &&
            load_sym(gtk, "gtk_container_add", gtk_container_add) &&
            load_sym(gtk, "gtk_widget_show_all", gtk_widget_show_all) &&
            load_sym(gtk, "gtk_main", gtk_main) &&
            load_sym(gtk, "gtk_main_quit", gtk_main_quit) &&
            load_sym(webkit, "webkit_web_view_new", webkit_web_view_new) &&
            load_sym(webkit, "webkit_web_view_load_uri", webkit_web_view_load_uri) &&
            load_sym(gobject, "g_signal_connect_data", g_signal_connect_data) &&
            load_sym(glib, "g_idle_add", g_idle_add);

        // Optional enhancements
        (void)load_sym(gtk, "gtk_settings_get_default", gtk_settings_get_default);
        (void)load_sym(gobject, "g_object_set", g_object_set);
        (void)load_sym(webkit, "webkit_web_view_set_background_color", webkit_web_view_set_background_color);
        return ok;
    }
};

std::mutex g_linux_mutex;
GtkMainQuitFn g_linux_quit_fn = nullptr;
GIdleAddFn g_linux_idle_add = nullptr;
bool g_linux_running = false;

int linux_idle_quit(void* /*data*/) {
    GtkMainQuitFn quit_fn = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_linux_mutex);
        quit_fn = g_linux_quit_fn;
    }
    if (quit_fn) {
        quit_fn();
    }
    return 0; // G_SOURCE_REMOVE
}

} // namespace

NativeWindowResult run_native_window(const NativeWindowOptions& opts) {
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if ((!display || display[0] == '\0') && (!wayland || wayland[0] == '\0')) {
        return NativeWindowResult::Unavailable;
    }

    LinuxRuntime rt;
    if (!rt.load()) {
        return NativeWindowResult::Unavailable;
    }

    if (!rt.gtk_init_check(nullptr, nullptr)) {
        return NativeWindowResult::Unavailable;
    }

    if (opts.dark_titlebar && rt.gtk_settings_get_default && rt.g_object_set) {
        GtkSettings* settings = rt.gtk_settings_get_default();
        if (settings) {
            rt.g_object_set(settings, "gtk-application-prefer-dark-theme", 1, static_cast<void*>(nullptr));
        }
    }

    GtkWidget* window = rt.gtk_window_new(0 /* GTK_WINDOW_TOPLEVEL */);
    if (!window) {
        return NativeWindowResult::Failed;
    }

    rt.gtk_window_set_title(window, opts.title.c_str());
    rt.gtk_window_set_default_size(window, opts.width, opts.height);
    rt.gtk_widget_set_size_request(window, opts.min_width, opts.min_height);

    GtkWidget* webview = rt.webkit_web_view_new();
    if (!webview) {
        return NativeWindowResult::Failed;
    }

    if (opts.dark_titlebar && rt.webkit_web_view_set_background_color) {
        GdkRGBA bg{11.0 / 255.0, 13.0 / 255.0, 16.0 / 255.0, 1.0};
        rt.webkit_web_view_set_background_color(webview, &bg);
    }

    rt.gtk_container_add(window, webview);
    rt.webkit_web_view_load_uri(webview, opts.url.c_str());

    rt.g_signal_connect_data(window, "destroy", rt.gtk_main_quit, nullptr, nullptr, 0);
    rt.gtk_widget_show_all(window);

    {
        std::lock_guard<std::mutex> lock(g_linux_mutex);
        g_linux_quit_fn = rt.gtk_main_quit;
        g_linux_idle_add = rt.g_idle_add;
        g_linux_running = true;
    }

    rt.gtk_main();

    {
        std::lock_guard<std::mutex> lock(g_linux_mutex);
        g_linux_running = false;
        g_linux_quit_fn = nullptr;
        g_linux_idle_add = nullptr;
    }

    return NativeWindowResult::Closed;
}

void request_native_window_close() {
    GIdleAddFn idle_add = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_linux_mutex);
        if (!g_linux_running) return;
        idle_add = g_linux_idle_add;
    }
    if (idle_add) {
        idle_add(&linux_idle_quit, nullptr);
    }
}

#endif

} // namespace loadout::surfaces
