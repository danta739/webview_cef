// webview_js_handler.cc — JS 桥实现。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_js_handler.h"

#include <sstream>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_v8.h"

namespace webview_cef {

namespace {
int g_next_request_id = 1;
}

bool WebviewJsHandler::Execute(const CefString& name,
                               CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception) {
    if (!cb_) return false;
    if (name != "cefQuery" || arguments.size() < 1) return false;

    auto arg = arguments[0];
    if (!arg || !arg->IsObject()) return false;

    auto req_val = arg->GetValue("request");
    auto success_val = arg->GetValue("onSuccess");
    auto failure_val = arg->GetValue("onFailure");

    if (!req_val || !req_val->IsObject()) {
        exception = "cefQuery: request must be an object";
        return true;
    }

    auto fun_name = req_val->GetValue("name");
    auto fun_param = req_val->GetValue("param");
    if (!fun_name || !fun_name->IsString()) {
        exception = "cefQuery: request.name must be a string";
        return true;
    }

    JsRequest req;
    req.name = fun_name->GetStringValue().ToString();
    req.param = fun_param && fun_param->IsString()
                    ? fun_param->GetStringValue().ToString()
                    : "";

    // 通过 V8 Context 拿到对应的 CefFrame,再发到浏览器进程。
    auto context = CefV8Context::GetCurrentContext();
    auto frame = context ? context->GetFrame() : nullptr;
    auto browser = frame ? frame->GetBrowser() : nullptr;
    if (!browser || !frame) {
        exception = "cefQuery: no frame";
        return true;
    }

    req.browser_id = static_cast<int>(browser->GetIdentifier());
    // CEF 149: frame->GetIdentifier() 现在返回 CefString。
    req.frame_id = 0;
    req.request_id = g_next_request_id++;

    cb_(req);
    retval = CefV8Value::CreateInt(req.request_id);
    return true;
}

void WebviewJsHandler::BindToContext(CefRefPtr<CefV8Value> window,
                                     CefRefPtr<WebviewJsHandler> handler) {
    if (!window || !handler) return;
    auto fn = CefV8Value::CreateFunction("cefQuery", handler.get());
    window->SetValue("cefQuery", fn, V8_PROPERTY_ATTRIBUTE_NONE);
}

}  // namespace webview_cef
