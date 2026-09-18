// webview_plugin.h — WebviewPlugin,WebView OSR 渲染的 C++ 端核心。
//
// 把 WebviewHandler 的回调打包成 MethodChannel 消息,
// 把 Dart 的方法调用翻译为 handler 的命令。

#ifndef WEBVIEW_PLUGIN_H
#define WEBVIEW_PLUGIN_H

#include "webview_value.h"
#include "webview_app.h"
#include "webview_handler.h"
#include <include/cef_base.h>

#include <functional>
#include <memory>

namespace webview_cef {

class WebviewTexture {
 public:
    virtual ~WebviewTexture() {}
    virtual void onFrame(const void* buffer, int width, int height) {}
    virtual void onAcceleratedFrame(const void* sharedHandle,
                                    int width, int height, int format) {}
    CefRefPtr<CefBrowser> browser;
    int64_t textureId = 0;
    bool isFocused = false;
    bool editableFocused = false;
    bool composing = false;
};

class WebviewPlugin {
 public:
    WebviewPlugin();
    ~WebviewPlugin();

    void initCallback();
    void uninitCallback();

    void HandleMethodCall(std::string name, WValue* values,
                          std::function<void(int, WValue*)> result);
    void sendKeyEvent(CefKeyEvent& ev);

    void setInvokeMethodFunc(std::function<void(std::string, WValue*)> func);
    void setCreateTextureFunc(std::function<std::shared_ptr<WebviewTexture>()> func);

    bool getAnyBrowserFocused();
    void tickBeginFrame();
    bool isEditableFocused();
    bool isComposing();

    void imeSetCompositionNative(const std::wstring& text, int cursor);
    void imeCommitTextNative(const std::wstring& text);
    void imeFinishCompositionNative();

 private:
    int focusedBrowserId();
    void setComposingForBrowser(int browserId, bool composing);
    int cursorAction(WValue* args, const std::string& name);

    std::function<void(std::string, WValue*)> m_invokeFunc;
    std::function<std::shared_ptr<WebviewTexture>()> m_createTextureFunc;
    CefRefPtr<WebviewHandler> m_handler;
    CefRefPtr<WebviewApp> m_app;
    std::unordered_map<int, std::shared_ptr<WebviewTexture>> m_renderers;
    bool m_init = false;
};

// 进程级 CEF 生命周期。
int initCEFProcesses(CefMainArgs args);
int initCEFProcesses();
void startCEF();
void doMessageLoopWork();
void SwapBufferFromBgraToRgba(void* _dest, const void* _src,
                              int width, int height);
void stopCEF();

}  // namespace webview_cef

#endif  // WEBVIEW_PLUGIN_H
