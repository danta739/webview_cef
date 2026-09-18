// webview_cef_plugin_c_api.cpp — C API 入口。
//
// 暴露 initCEFProcesses / handleWndProcForCEF 给 Flutter runner 直接调用。

#include "include/webview_cef/webview_cef_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>
#include <cstdio>
#include <string>
#include <memory>

#include "webview_cef_plugin.h"
#include "webview_cef_windowed_plugin.h"

namespace {

// 全局持有 WindowedWebviewPlugin 实例。
std::unique_ptr<webview_cef::WindowedWebviewPlugin> g_windowed_plugin;

}  // namespace

extern "C" void WebviewCefPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
    // 把 C-API 句柄包装成 C++ 端 PluginRegistrar 后分发。
    // 用 GetInstance()->GetRegistrar 创建一个 Windows wrapper,直接传给两个
    // plugin 的 RegisterWithRegistrar。
    auto* windows_registrar =
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar);
    if (!windows_registrar) return;
    webview_cef::WebviewCefPlugin::RegisterWithRegistrar(windows_registrar);

    if (!g_windowed_plugin) {
        HWND parent_hwnd = nullptr;
        auto* view = windows_registrar->GetView();
        if (view) {
            parent_hwnd = view->GetNativeWindow();
        }
        g_windowed_plugin =
            std::make_unique<webview_cef::WindowedWebviewPlugin>(
                windows_registrar, parent_hwnd);
        g_windowed_plugin->BindChannel();
    }
}

extern "C" int initCEFProcesses(HINSTANCE hInstance) {
    // 仅使用 OutputDebugString —— 不会触发 conhost 黑窗口。
    // 子进程(GPU / renderer / utility 等)也会经过这里,
    // 但只调 CefExecuteProcess 后立即返回,不接触任何 console API。
    auto log = [](const std::string& msg) {
        OutputDebugStringA(msg.c_str());
    };
    log("initCEFProcesses: enter hInstance=" + std::to_string((uintptr_t)hInstance));
    CefMainArgs main_args(hInstance);
    int rc = webview_cef::initCEFProcesses(main_args);
    log("initCEFProcesses: CefExecuteProcess returned " + std::to_string(rc));
    return rc;
}

extern "C" void handleWndProcForCEF(HWND hwnd, unsigned int message,
                                    unsigned __int64 wParam,
                                    __int64 lParam) {
    // 把 Windows 消息转发给 CEF 处理(用于 CEF 键盘输入和 IME)。
    (void)hwnd; (void)message; (void)wParam; (void)lParam;
}
