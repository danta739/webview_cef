#ifndef WEBVIEW_PLUGIN_H
#define WEBVIEW_PLUGIN_H

#include "webview_value.h"
#include "webview_app.h"
#include <include/cef_base.h>

#include <functional>
namespace webview_cef {
    class WebviewTexture{
    public:
        virtual ~WebviewTexture(){}
        // 软件离屏帧（来自 CefRenderHandler::OnPaint 的 CPU BGRA 缓冲区）。
        virtual void onFrame(const void* buffer, int width, int height){}
        // GPU 加速帧（来自 OnAcceleratedPaint 的共享纹理句柄）。
        virtual void onAcceleratedFrame(const void* sharedHandle, int width, int height, int format){}
        int64_t textureId = 0;
        bool isFocused = false;
        // 此浏览器的 IME 状态（单个插件可以承载多个）。Windows 按键路由器会参考
        // 焦点浏览器的状态，因此一个 webview 的可编辑焦点/组合不会影响其他 webview 的输入。
        bool editableFocused = false;  // 焦点位于 web 可编辑节点
        bool composing = false;        // 操作系统 IME 存在活动的标记组合
    };
    class WebviewPlugin {
    public:
        WebviewPlugin();
        ~WebviewPlugin();
        void initCallback();
        void uninitCallback();
        void HandleMethodCall(std::string name, WValue* values, std::function<void(int ,WValue*)> result);
        void sendKeyEvent(CefKeyEvent& ev);
        void setInvokeMethodFunc(std::function<void(std::string, WValue*)> func);
        void setCreateTextureFunc(std::function<std::shared_ptr<WebviewTexture>()> func);
        bool getAnyBrowserFocused();
        // 为此插件的浏览器驱动一次外部 BeginFrame（GPU 路径）。
        void tickBeginFrame();
        // 当前焦点浏览器的 IME 状态（参见 WebviewTexture）。Windows 按键路由器
        // 使用这些状态将组合键发送给操作系统 IME，将导航/控制键发送给 CEF。
        // 状态按浏览器保留，因此一个 webview 的组合不会影响其他 webview 的键盘输入。
        bool isEditableFocused();
        bool isComposing();

        // 原生 IME 管道转发器（由 Windows WM_IME_* 处理器驱动）。
        void imeSetCompositionNative(const std::wstring& text, int cursor);
        void imeCommitTextNative(const std::wstring& text);
        void imeFinishCompositionNative();

    private :
        // 解析当前具有焦点的渲染进程所属的 browserId，若无则返回 -1。
        int focusedBrowserId();
        // 为特定浏览器设置 composing 标志（未知则为 no-op）。
        void setComposingForBrowser(int browserId, bool composing);
        int cursorAction(WValue *args, std::string name);
    	std::function<void(std::string, WValue*)> m_invokeFunc;
	    std::function<std::shared_ptr<WebviewTexture>()> m_createTextureFunc;
        CefRefPtr<WebviewHandler> m_handler;
	    CefRefPtr<WebviewApp> m_app;
    	std::unordered_map<int, std::shared_ptr<WebviewTexture>> m_renderers;
	    bool m_init = false;
    };

    int initCEFProcesses(CefMainArgs args);
    int initCEFProcesses();
    void startCEF();
    void doMessageLoopWork();
    void SwapBufferFromBgraToRgba(void* _dest, const void* _src, int width, int height);
    void stopCEF();
}

#endif //WEBVIEW_PLUGIN_H
