// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

#include <functional>
#include <list>
#include <unordered_map>

#include "webview_cookieVisitor.h"

#define ColorUNDERLINE \
  0xFF000000  // 用于下划线的黑色 SkColor 值，
              // 与 Blink 相同。
#define ColorBKCOLOR \
  0x00000000  // 用于背景的白色 SkColor 值，
              // 与 Blink 相同。

struct browser_info{
    CefRefPtr<CefBrowser> browser;
    uint32_t width = 1;
    uint32_t height = 1;
    float dpi = 1.0;
    bool is_dragging = false;
    CefRect prev_ime_position = CefRect();
    bool is_ime_commit = false;
    // 宿主请求的焦点（通过 setClientFocus），以及在第一帧渲染后是否已重新声明。
    // 使用 external_begin_frame 时，浏览器在帧流通之前不会处于输入/焦点就绪状态，
    // 因此在创建后立即发出的 SetFocus 会丢失；我们在第一帧落地后重新应用它。
    bool wants_focus = false;
    bool focus_reasserted = false;
};

class WebviewHandler : public CefClient,
public CefDisplayHandler,
public CefLifeSpanHandler,
public CefFocusHandler,
public CefLoadHandler,
public CefRenderHandler{
public:
    //绘制回调（软件离屏渲染）
    std::function<void(int browserId, const void* buffer, int32_t width, int32_t height)> onPaintCallback;
    //加速绘制回调（GPU 共享纹理）。在 Windows 上 |sharedHandle| 是 OnAcceleratedPaint
    //的共享纹理 HANDLE；|format| 是 cef_color_type_t。仅在启用共享纹理时触发。
    std::function<void(int browserId, const void* sharedHandle, int32_t width, int32_t height, int32_t format)> onAcceleratedPaintCallback;
    //cef 消息事件
    std::function<void(int browserId, std::string url)> onUrlChangedEvent;
    std::function<void(int browserId, std::string title)> onTitleChangedEvent;
    std::function<void(int browserId, int type)>onCursorChangedEvent;
    std::function<void(int browserId, std::string text)> onTooltipEvent;
    std::function<void(int browserId, int level, std::string message, std::string source, int line)>onConsoleMessageEvent;
    std::function<void(int browserId, bool editable)> onFocusedNodeChangeMessage;
    std::function<void(int browserId, int32_t x, int32_t y, int32_t height)> onImeCompositionRangeChangedMessage;
    //网页消息
    std::function<void(std::string, std::string, std::string, int browserId, std::string)> onJavaScriptChannelMessage;
    std::function<void(int browserId, std::string url)> onLoadStart;
    std::function<void(int browserId, std::string url)> onLoadEnd;
    
    explicit WebviewHandler();
    ~WebviewHandler();
    
    // CefClient methods:
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefFocusHandler> GetFocusHandler() override {
        return this;
    }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

	bool OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

    
    // CefDisplayHandler methods:
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                               const CefString& title) override;
    virtual void OnAddressChange(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& url) override;
    virtual bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                                CefCursorHandle cursor,
                                cef_cursor_type_t type,
                                const CefCursorInfo& custom_cursor_info) override;
    virtual bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) override;
    virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                  cef_log_severity_t level,
                                  const CefString& message,
                                  const CefString& source,
                                  int line) override;
    
    // CefLifeSpanHandler methods:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int popup_id,
                               const CefString& target_url,
                               const CefString& target_frame_name,
                               WindowOpenDisposition target_disposition,
                               bool user_gesture,
                               const CefPopupFeatures& popupFeatures,
                               CefWindowInfo& windowInfo,
                               CefRefPtr<CefClient>& client,
                               CefBrowserSettings& settings,
                               CefRefPtr<CefDictionaryValue>& extra_info,
                               bool* no_javascript_access) override;
    
    virtual void OnTakeFocus(CefRefPtr<CefBrowser> browser, bool next) override;
    virtual bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override;
    virtual void OnGotFocus(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler methods:
    virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode,
                             const CefString& errorText,
                             const CefString& failedUrl) override;
    virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) override;
    virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefLoadHandler::TransitionType transition_type) override;
    
    // CefRenderHandler methods:
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;
    virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const CefAcceleratedPaintInfo& info) override;
    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) override;
    virtual bool StartDragging(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefDragData> drag_data,
                               DragOperationsMask allowed_ops,
                               int x,
                               int y) override;
    virtual void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,const CefRange& selection_range,const CefRenderHandler::RectList& character_bounds) override;

    // 请求关闭所有现有的浏览器窗口。
    void CloseAllBrowsers(bool force_close);

    void closeBrowser(int browserId);
    void createBrowser(std::string url, std::function<void(int)> callback);

    // 为每个活动浏览器驱动一次外部 BeginFrame（仅限 GPU 路径）。
    // 编组到 CEF UI 线程；可从任何线程调用。
    void sendExternalBeginFrame();

    // 诊断（GPU 路径）：如果浏览器创建后短时间内没有加速绘制帧到达则警告一次 —
    // 这是 GPU 共享纹理不可用的症状，否则将导致 webview 静默变黑。
    void warnIfNoAcceleratedFrame();

    void sendScrollEvent(int browserId, int x, int y, int deltaX, int deltaY);
    void changeSize(int browserId, float a_dpi, int width, int height);
    void cursorClick(int browserId, int x, int y, bool up);
    void cursorMove(int browserId, int x, int y, bool dragging);
    void sendKeyEvent(CefKeyEvent& ev);
    void loadUrl(int browserId, std::string url);
    void goForward(int browserId);
    void goBack(int browserId);
    void reload(int browserId);
    void openDevTools(int browserId);

    void imeSetComposition(int browserId, std::string text);
    void imeCommitText(int browserId, std::string text);
    void setClientFocus(int browserId, bool focus);

    // 原生 IME 管道（Windows WM_IME_*）：对当前获得焦点的浏览器进行操作。
    // |text| 是从 IMM 组合字符串读取的 UTF-16。
    CefRefPtr<CefBrowser> getFocusedBrowser();
    void imeSetCompositionNative(const std::wstring& text, int cursor);
    void imeCommitTextNative(const std::wstring& text);
    void imeFinishComposition();

    void setCookie(const std::string& domain, const std::string& key, const std::string& value);
    void deleteCookie(const std::string& domain, const std::string& key);
    void visitAllCookies(std::function<void(std::map<std::string, std::map<std::string, std::string>>)> callback);
    void visitUrlCookies(const std::string& domain, const bool& isHttpOnly, std::function<void(std::map<std::string, std::map<std::string, std::string>>)> callback);

    void setJavaScriptChannels(int browserId, const std::vector<std::string> channels);
    void sendJavaScriptChannelCallBack(const bool error, const std::string result, const std::string callbackId, const int browserId, const std::string frameId);
    void executeJavaScript(int browserId, const std::string code, std::function<void(CefRefPtr<CefValue>)> callback = nullptr);
    
private:
    // 现有浏览器窗口列表。仅在 CEF UI 线程上访问。
    std::unordered_map<int, browser_info> browser_map_;

    std::unordered_map<std::string, std::function<void(CefRefPtr<CefValue>)>> js_callbacks_;

#ifdef WEBVIEW_CEF_GPU_TEXTURE
    // GPU 诊断状态：是否有任何加速绘制帧到达，以及"无 GPU 帧"警告是否
    // 已经记录（仅记录一次）。仅在 GPU 构建中声明，以避免非 GPU 构建中出现未使用字段。
    bool received_accelerated_frame_ = false;
    bool gpu_warning_logged_ = false;
#endif

    // 包含默认的引用计数实现。
    IMPLEMENT_REFCOUNTING(WebviewHandler);

};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
