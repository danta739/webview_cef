// webview_app.h — CEF App 实现。
//
// 负责 CEF 子进程分派、命令行解析、JS handler 注册。
// 与 OSR / Windowed 都无关,只是 CEF 进程模型的入口。

#ifndef WEBVIEW_APP_H
#define WEBVIEW_APP_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cef_app.h"

namespace webview_cef {

class WebviewApp : public CefApp,
                   public CefBrowserProcessHandler {
 public:
    WebviewApp() = default;

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override;

    void OnContextInitialized() override;

 private:
    IMPLEMENT_REFCOUNTING(WebviewApp);
};

}  // namespace webview_cef

#endif  // WEBVIEW_APP_H
