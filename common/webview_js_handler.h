// webview_js_handler.h — JavaScript ↔ Dart 桥。
//
// 在每个 V8 context 上注入两个对象:
//   * window.cefQuery:页面调 cefQuery({ request, onSuccess, onFailure })
//     → 把请求发回 Dart 侧。
//   * CefClient.Print(name, message, callbackId):反向,Dart 调用后页面收到。

#ifndef WEBVIEW_JS_HANDLER_H
#define WEBVIEW_JS_HANDLER_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <functional>

#include "cef_v8.h"

namespace webview_cef {

// 来自 JS 的请求,由 JS handler 解析后回调到上层。
struct JsRequest {
    int browser_id = 0;
    int frame_id = 0;
    std::string name;        // 调用方提供的函数名
    std::string param;       // JSON 字符串
    int request_id = 0;      // 用于回传 callback
};

// 上层注入的回调:把请求转给 Dart。
using JsRequestCallback = std::function<void(const JsRequest&)>;

class WebviewJsHandler : public CefV8Handler {
 public:
    explicit WebviewJsHandler(JsRequestCallback cb) : cb_(std::move(cb)) {}

    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override;

    // 把 window 上的函数绑定到指定 context。
    static void BindToContext(CefRefPtr<CefV8Value> window,
                              CefRefPtr<WebviewJsHandler> handler);

 private:
    JsRequestCallback cb_;

    IMPLEMENT_REFCOUNTING(WebviewJsHandler);
};

}  // namespace webview_cef

#endif  // WEBVIEW_JS_HANDLER_H
