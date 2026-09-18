// webview_handler.cc — OSR CefClient 实现。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_handler.h"

#include <sstream>
#include <string>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <cstdint>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "webview_js_handler.h"

namespace webview_cef {

namespace {

std::string to_string_int(int n) {
    std::ostringstream o;
    o << n;
    return o.str();
}

constexpr char kFocusedNodeChangedMessage[] = "FocusedNodeChanged";
constexpr char kJSCallCppFunctionMessage[]   = "JSCallCppFunction";
constexpr char kEvaluateCallbackMessage[]    = "EvaluateCallback";

}  // namespace

WebviewHandler::WebviewHandler() = default;

WebviewHandler::~WebviewHandler() {
    browser_map_.clear();
}

bool WebviewHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
    std::string message_name = message->GetName();
    if (message_name == kFocusedNodeChangedMessage) {
        bool editable = message->GetArgumentList()->GetBool(0);
        onFocusedNodeChangeMessage(browser->GetIdentifier(), editable);
        if (editable) {
            onImeCompositionRangeChangedMessage(
                browser->GetIdentifier(),
                message->GetArgumentList()->GetInt(1),
                message->GetArgumentList()->GetInt(2),
                message->GetArgumentList()->GetInt(3));
        }
    } else if (message_name == kJSCallCppFunctionMessage) {
        CefString fun_name = message->GetArgumentList()->GetString(0);
        CefString param = message->GetArgumentList()->GetString(1);
        int js_callback_id = message->GetArgumentList()->GetInt(2);
        if (fun_name.empty() || !browser.get()) return false;
        onJavaScriptChannelMessage(
            fun_name, param,
            to_string_int(js_callback_id),
            browser->GetIdentifier(),
            frame ? frame->GetIdentifier().ToString() : "");
    } else if (message_name == kEvaluateCallbackMessage) {
        CefString callbackId = message->GetArgumentList()->GetString(0);
        // 评估回调由上层实现处理,这里留空。
        (void)callbackId;
    }
    return false;
}

void WebviewHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
    if (onTitleChangedEvent) {
        onTitleChangedEvent(browser->GetIdentifier(), title);
    }
}

void WebviewHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const CefString& url) {
    if (onUrlChangedEvent) {
        onUrlChangedEvent(browser->GetIdentifier(), url);
    }
}

bool WebviewHandler::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                    CefCursorHandle cursor,
                                    cef_cursor_type_t type,
                                    const CefCursorInfo& custom_cursor_info) {
    if (onCursorChangedEvent) {
        onCursorChangedEvent(browser->GetIdentifier(), static_cast<int>(type));
    }
    return false;
}

bool WebviewHandler::OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) {
    if (onTooltipEvent) {
        onTooltipEvent(browser->GetIdentifier(), text);
    }
    return false;
}

bool WebviewHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                      cef_log_severity_t level,
                                      const CefString& message,
                                      const CefString& source, int line) {
    if (onConsoleMessageEvent) {
        onConsoleMessageEvent(browser->GetIdentifier(),
                              static_cast<int>(level), message,
                              source, line);
    }
    return false;
}

void WebviewHandler::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selected_range,
    const RectList& character_bounds) {
    // 占位:TextInput 状态由上层 WebviewPlugin 维护。
    (void)browser;
    (void)selected_range;
    (void)character_bounds;
}

void WebviewHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    if (!browser) return;
    int id = browser->GetIdentifier();
    browser_info info;
    info.browser = browser;
    browser_map_[id] = info;
}

bool WebviewHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    return false;
}

void WebviewHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    if (!browser) return;
    int id = browser->GetIdentifier();
    auto it = browser_map_.find(id);
    if (it != browser_map_.end()) {
        it->second.browser = nullptr;
        browser_map_.erase(it);
    }
}

void WebviewHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 TransitionType transition_type) {
    if (!browser) return;
    if (onLoadStart) {
        onLoadStart(browser->GetIdentifier(),
                    frame ? frame->GetURL().ToString() : "");
    }
}

void WebviewHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
    if (!browser) return;
    if (onLoadEnd) {
        onLoadEnd(browser->GetIdentifier(),
                  frame ? frame->GetURL().ToString() : "");
    }
}

void WebviewHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it == browser_map_.end()) {
        rect = CefRect(0, 0, 1, 1);
        return;
    }
    rect = CefRect(0, 0,
                   static_cast<int>(it->second.width),
                   static_cast<int>(it->second.height));
}

void WebviewHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                             PaintElementType type,
                             const RectList& dirtyRects,
                             const void* buffer, int width, int height) {
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it == browser_map_.end()) return;
    it->second.width = static_cast<uint32_t>(width);
    it->second.height = static_cast<uint32_t>(height);
    if (onPaintCallback) {
        onPaintCallback(browser->GetIdentifier(), buffer, width, height);
    }
}

void WebviewHandler::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                        PaintElementType type,
                                        const RectList& dirtyRects,
                                        const CefAcceleratedPaintInfo& info) {
    auto it = browser_map_.find(browser->GetIdentifier());
    if (it == browser_map_.end()) return;
    if (onAcceleratedPaintCallback) {
        // CEF 149: 通过 info.shared_texture_handle 拿到底层 handle。
        void* shared_handle = const_cast<void*>(info.shared_texture_handle);
        onAcceleratedPaintCallback(browser->GetIdentifier(),
                                   shared_handle,
                                   static_cast<int32_t>(it->second.width),
                                   static_cast<int32_t>(it->second.height),
                                   0);
    }
    (void)type;
    (void)dirtyRects;
}

bool WebviewHandler::StartDragging(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefDragData> drag_data,
                                   CefRenderHandler::DragOperationsMask allowed_ops,
                                   int x, int y) {
    return false;
}

void WebviewHandler::UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                                      CefRenderHandler::DragOperation operation) {}

void WebviewHandler::createBrowser(const std::string& url,
                                   std::function<void(int)> callback) {
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);  // OSR 模式,parent = 0
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;

    auto handler = this;
    CefRefPtr<CefBrowser> browser =
        CefBrowserHost::CreateBrowserSync(
            window_info, handler, CefString(url), browser_settings,
            /*extra_info=*/nullptr, /*request_context=*/nullptr);
    if (browser && callback) {
        callback(browser->GetIdentifier());
    }
}

void WebviewHandler::closeBrowser(int browserId) {
    auto it = browser_map_.find(browserId);
    if (it == browser_map_.end()) return;
    if (it->second.browser) {
        it->second.browser->GetHost()->CloseBrowser(true);
    }
}

std::vector<int> WebviewHandler::getActiveBrowserIds() const {
    std::vector<int> ids;
    ids.reserve(browser_map_.size());
    for (auto& kv : browser_map_) ids.push_back(kv.first);
    return ids;
}

}  // namespace webview_cef
