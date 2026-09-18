// webview_cef_windowed_plugin.cpp — 屏上渲染 MethodChannel 桥接。

#include "webview_cef_windowed_plugin.h"

#include <flutter_windows.h>

#include <iostream>
#include <memory>
#include <utility>

#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"

namespace webview_cef {

namespace {

int GetInt(const flutter::EncodableValue* v, const std::string& key,
           int default_val = 0) {
    if (!v) return default_val;
    auto* map = std::get_if<flutter::EncodableMap>(v);
    if (!map) return default_val;
    auto it = map->find(flutter::EncodableValue(key));
    if (it == map->end()) return default_val;
    if (auto* p = std::get_if<int32_t>(&it->second)) return *p;
    if (auto* p = std::get_if<int64_t>(&it->second))
        return static_cast<int>(*p);
    return default_val;
}

std::string GetString(const flutter::EncodableValue* v, const std::string& key) {
    if (!v) return "";
    auto* map = std::get_if<flutter::EncodableMap>(v);
    if (!map) return "";
    auto it = map->find(flutter::EncodableValue(key));
    if (it == map->end()) return "";
    if (auto* p = std::get_if<std::string>(&it->second)) return *p;
    return "";
}

}  // namespace

WindowedWebviewPlugin::WindowedWebviewPlugin(flutter::PluginRegistrar* registrar,
                                              HWND parent_hwnd)
    : registrar_(registrar), parent_hwnd_(parent_hwnd) {}

WindowedWebviewPlugin::~WindowedWebviewPlugin() {
    Shutdown();
}

void WindowedWebviewPlugin::SendEvent(const std::string& name,
                                      flutter::EncodableValue payload) {
    if (!channel_) return;
    auto args = std::make_unique<flutter::EncodableValue>(std::move(payload));
    channel_->InvokeMethod(name, std::move(args));
}

void WindowedWebviewPlugin::BindChannel() {
    channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        registrar_->messenger(), "webview_cef_windowed",
        &flutter::StandardMethodCodec::GetInstance());

    auto self_ptr = this;
    channel_->SetMethodCallHandler(
        [self_ptr](const auto& call, auto result) {
            self_ptr->HandleMethodCall(call, std::move(result));
        });

    if (parent_hwnd_) {
        webview_windowed::BindParentWindow(parent_hwnd_);
    }
}

void WindowedWebviewPlugin::Shutdown() {
    if (channel_) {
        channel_->SetMethodCallHandler(nullptr);
        channel_.reset();
    }
    CefPostTask(TID_UI, base::BindOnce([]() {
        webview_windowed::CloseAllWindowedBrowsers(true);
        webview_windowed::UnbindParentWindow();
    }));
    active_browser_id_ = -1;
}

void WindowedWebviewPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

    if (!parent_hwnd_) {
        result->Error("NO_PARENT", "Parent HWND not available");
        return;
    }

    const flutter::EncodableValue* args = call.arguments();
    const std::string& method = call.method_name();

    if (method == "createWindowed") {
        std::string url = GetString(args, "url");
        if (url.empty()) url = "about:blank";
        int x = GetInt(args, "x", 0);
        int y = GetInt(args, "y", 0);
        int w = GetInt(args, "w", 800);
        int h = GetInt(args, "h", 600);

        HWND parent = parent_hwnd_;
        CefString cef_url(url);
        webview_windowed::WindowedPlacement placement{x, y, w, h};

        auto cb = std::move(result);
        auto* plugin_ptr = this;

        CefPostTask(TID_UI, base::BindOnce(
            [](HWND p, CefString u, webview_windowed::WindowedPlacement plc,
               std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> r,
               WindowedWebviewPlugin* plugin,
               int* out_id) {
                webview_windowed::WindowedCallbacks cb;
                cb.on_url_changed = [plugin](int bid, const std::string& url) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("url")] =
                        flutter::EncodableValue(url);
                    plugin->SendEvent("onUrlChanged",
                                      flutter::EncodableValue(std::move(m)));
                };
                cb.on_title_changed = [plugin](int bid, const std::string& title) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("title")] =
                        flutter::EncodableValue(title);
                    plugin->SendEvent("onTitleChanged",
                                      flutter::EncodableValue(std::move(m)));
                };
                cb.on_load_start = [plugin](int bid, const std::string& url) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("url")] =
                        flutter::EncodableValue(url);
                    plugin->SendEvent("onLoadStart",
                                      flutter::EncodableValue(std::move(m)));
                };
                cb.on_load_end = [plugin](int bid, const std::string& url) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("url")] =
                        flutter::EncodableValue(url);
                    plugin->SendEvent("onLoadEnd",
                                      flutter::EncodableValue(std::move(m)));
                };
                cb.on_console_message = [plugin](int bid, int level,
                                                 const std::string& message,
                                                 const std::string& source,
                                                 int line) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("level")] =
                        flutter::EncodableValue(level);
                    m[flutter::EncodableValue("message")] =
                        flutter::EncodableValue(message);
                    m[flutter::EncodableValue("source")] =
                        flutter::EncodableValue(source);
                    m[flutter::EncodableValue("line")] =
                        flutter::EncodableValue(line);
                    plugin->SendEvent("onConsoleMessage",
                                      flutter::EncodableValue(std::move(m)));
                };
                cb.on_browser_closed = [plugin](int bid) {
                    flutter::EncodableMap m;
                    m[flutter::EncodableValue("browserId")] =
                        flutter::EncodableValue(bid);
                    plugin->SendEvent("onBrowserClosed",
                                      flutter::EncodableValue(std::move(m)));
                };

                auto browser = webview_windowed::CreateWindowedBrowser(p, u, plc, cb);
                if (!browser) {
                    r->Error("CREATE_FAILED",
                             "CEF CreateBrowserSync returned null");
                    return;
                }
                int id = browser->GetIdentifier();
                if (out_id) *out_id = id;

                flutter::EncodableMap reply;
                reply[flutter::EncodableValue("browserId")] =
                    flutter::EncodableValue(id);
                r->Success(flutter::EncodableValue(std::move(reply)));
            },
            parent, cef_url, placement, std::move(cb),
            plugin_ptr, &active_browser_id_));

    } else if (method == "updateWindowedRect") {
        int x = GetInt(args, "x", 0);
        int y = GetInt(args, "y", 0);
        int w = GetInt(args, "w", 0);
        int h = GetInt(args, "h", 0);
        int bid = active_browser_id_;

        auto cb = std::move(result);

        CefPostTask(TID_UI, base::BindOnce(
            [](int b, int xx, int yy, int ww, int hh,
               std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> r) {
                webview_windowed::SyncChildRectTo(b, xx, yy, ww, hh);
                r->Success();
            },
            bid, x, y, w, h, std::move(cb)));

    } else if (method == "closeWindowed") {
        int bid = active_browser_id_;
        auto cb = std::move(result);
        CefPostTask(TID_UI, base::BindOnce(
            [](int b, std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> r) {
                webview_windowed::CloseWindowedBrowser(b);
                r->Success();
            },
            bid, std::move(cb)));
        active_browser_id_ = -1;

    } else if (method == "isBound") {
        bool bound = webview_windowed::IsWindowedModeBound();
        flutter::EncodableMap reply;
        reply[flutter::EncodableValue("bound")] = flutter::EncodableValue(bound);
        result->Success(flutter::EncodableValue(std::move(reply)));

    } else if (method == "notifyResized") {
        webview_windowed::NotifyParentResized();
        result->Success();

    } else if (method == "loadUrl") {
        std::string url = GetString(args, "url");
        int bid = active_browser_id_;
        auto cb = std::move(result);
        CefPostTask(TID_UI, base::BindOnce(
            [](int b, std::string u,
               std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> r) {
                auto client = webview_windowed::GetWindowedClient(b);
                if (!client || !client->GetBrowser().get()) {
                    r->Error("NO_BROWSER", "Windowed browser not found");
                    return;
                }
                client->GetBrowser()->GetMainFrame()->LoadURL(CefString(u));
                r->Success();
            },
            bid, url, std::move(cb)));

    } else if (method == "goBack" || method == "goForward" ||
               method == "reload" || method == "openDevTools") {
        int bid = active_browser_id_;
        std::string op = method;
        auto cb = std::move(result);
        CefPostTask(TID_UI, base::BindOnce(
            [](int b, std::string op,
               std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> r) {
                auto client = webview_windowed::GetWindowedClient(b);
                if (!client) {
                    r->Error("NO_BROWSER", "Windowed client not found");
                    return;
                }
                auto browser = client->GetBrowser();
                if (!browser.get()) {
                    r->Error("NO_BROWSER", "Windowed browser not found");
                    return;
                }
                if (op == "goBack") browser->GoBack();
                else if (op == "goForward") browser->GoForward();
                else if (op == "reload") browser->Reload();
                else if (op == "openDevTools") {
                    browser->GetHost()->ShowDevTools(
                        CefWindowInfo(), client.get(),
                        CefBrowserSettings(), CefPoint());
                }
                r->Success();
            },
            bid, op, std::move(cb)));

    } else {
        result->NotImplemented();
    }
}

}  // namespace webview_cef
