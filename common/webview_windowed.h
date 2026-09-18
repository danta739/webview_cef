// webview_windowed.h — 屏上渲染(Windowed Mode)子窗口同步模块。
//
// 目标:在宿主窗口(HWND)上叠加一个 CEF 子窗口,跟随宿主的
// resize / move / DPI 变化实时调整位置和尺寸。
//
// 重要约束:
//   - 与现有 OSR 路径完全并存,不修改 webview_handler.cc 任何代码。
//   - 仅在宿主进程主动选择屏上渲染模式时调用本模块的 API。
//   - 所有函数必须在 CEF UI 线程上调用(除了 BindParentWindow / NotifyParentResized)。

#ifndef WEBVIEW_WINDOWED_H_
#define WEBVIEW_WINDOWED_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "cef_browser.h"
#include "cef_client.h"

namespace webview_windowed {

struct WindowedPlacement {
    int x = 0;
    int y = 0;
    int width = 800;
    int height = 600;
};

struct WindowedCallbacks {
    std::function<void(int browser_id, const std::string& url)> on_url_changed;
    std::function<void(int browser_id, const std::string& title)> on_title_changed;
    std::function<void(int browser_id, const std::string& url)> on_load_start;
    std::function<void(int browser_id, const std::string& url)> on_load_end;
    std::function<void(int browser_id, int level, const std::string& message,
                       const std::string& source, int line)> on_console_message;
    std::function<void(int browser_id)> on_browser_closed;
};

class WindowedClient : public CefClient,
                       public CefLifeSpanHandler,
                       public CefLoadHandler,
                       public CefDisplayHandler {
 public:
    void SetBrowser(CefRefPtr<CefBrowser> browser) { browser_ = browser; }
    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;
    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override;
    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override;
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override;

    explicit WindowedClient(WindowedCallbacks callbacks);

 private:
    WindowedCallbacks callbacks_;
    CefRefPtr<CefBrowser> browser_;

    IMPLEMENT_REFCOUNTING(WindowedClient);
};

struct WindowedChild {
    CefRefPtr<CefBrowser> browser;
    HWND hwnd = nullptr;
    CefRefPtr<WindowedClient> client;
};

// 任意线程。
void BindParentWindow(HWND parent_hwnd);
void UnbindParentWindow();
void NotifyParentResized();

// 必须在 CEF UI 线程。
CefRefPtr<CefBrowser> CreateWindowedBrowser(
    HWND parent_hwnd,
    const CefString& url,
    const WindowedPlacement& placement,
    const WindowedCallbacks& callbacks);

void SyncChildRect(int browser_id);
void SyncChildRectTo(int browser_id, int x, int y, int w, int h);

void CloseWindowedBrowser(int browser_id);
void CloseAllWindowedBrowsers(bool force);

bool IsWindowedModeBound();

CefRefPtr<WindowedClient> GetWindowedClient(int browser_id);

}  // namespace webview_windowed

#endif  // WEBVIEW_WINDOWED_H_
