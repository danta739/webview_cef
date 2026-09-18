// webview_handler.h — 离屏渲染(OSR) CefClient 实现。
//
// 实现多个 Cef*Handler 接口,把帧、URL、title、load 事件等转发给上层。
// 上层(WebviewPlugin)会把这些回调再转换成 MethodChannel 消息发给 Dart。

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cef_client.h"

#include <functional>
#include <list>
#include <unordered_map>

#include "webview_cookieVisitor.h"

#define ColorUNDERLINE 0xFF000000
#define ColorBKCOLOR   0x00000000

struct browser_info {
    CefRefPtr<CefBrowser> browser;
    uint32_t width = 1;
    uint32_t height = 1;
    float dpi = 1.0f;
    bool is_dragging = false;
    CefRect prev_ime_position = CefRect();
    bool is_ime_commit = false;
    bool wants_focus = false;
    bool focus_reasserted = false;
};

namespace webview_cef {

class WebviewHandler : public CefClient,
                       public CefDisplayHandler,
                       public CefLifeSpanHandler,
                       public CefFocusHandler,
                       public CefLoadHandler,
                       public CefRenderHandler {
 public:
    // 帧回调(软件 OSR)。
    std::function<void(int browserId, const void* buffer,
                       int32_t width, int32_t height)> onPaintCallback;
    // GPU 共享纹理帧(可选)。
    std::function<void(int browserId, const void* sharedHandle,
                       int32_t width, int32_t height, int32_t format)>
        onAcceleratedPaintCallback;
    std::function<void(int browserId, std::string url)> onUrlChangedEvent;
    std::function<void(int browserId, std::string title)> onTitleChangedEvent;
    std::function<void(int browserId, int type)> onCursorChangedEvent;
    std::function<void(int browserId, std::string text)> onTooltipEvent;
    std::function<void(int browserId, int level, std::string message,
                       std::string source, int line)> onConsoleMessageEvent;
    std::function<void(int browserId, bool editable)> onFocusedNodeChangeMessage;
    std::function<void(int browserId, int32_t x, int32_t y, int32_t height)>
        onImeCompositionRangeChangedMessage;
    std::function<void(std::string, std::string, std::string, int,
                       std::string)> onJavaScriptChannelMessage;
    std::function<void(int browserId, std::string url)> onLoadStart;
    std::function<void(int browserId, std::string url)> onLoadEnd;

    explicit WebviewHandler();
    ~WebviewHandler();

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefProcessId source_process,
                                 CefRefPtr<CefProcessMessage> message) override;

    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override;
    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override;
    bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                        CefCursorHandle cursor,
                        cef_cursor_type_t type,
                        const CefCursorInfo& custom_cursor_info) override;
    bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) override;
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source, int line) override;
    void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                      const CefRange& selected_range,
                                      const RectList& character_bounds) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width,
                 int height) override;
    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const CefAcceleratedPaintInfo& info) override;
    bool StartDragging(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefDragData> drag_data,
                       CefRenderHandler::DragOperationsMask allowed_ops,
                       int x, int y) override;
    void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::DragOperation operation) override;

    // 创建浏览器(OSR 模式)。
    void createBrowser(const std::string& url,
                       std::function<void(int)> callback);

    // 关闭浏览器。
    void closeBrowser(int browserId);

    // 查询所有活动浏览器。
    std::vector<int> getActiveBrowserIds() const;

    // 注意:WebviewHandler 不继承 CefRequestHandler。
    // 若需自定义弹窗/导航,另写一个独立 handler。

 private:
    std::unordered_map<int, browser_info> browser_map_;
    std::list<CefRefPtr<CefCookieManager>> cookie_managers_;

    IMPLEMENT_REFCOUNTING(WebviewHandler);
};

}  // namespace webview_cef

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
