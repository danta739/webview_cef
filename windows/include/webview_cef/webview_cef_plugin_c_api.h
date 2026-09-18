// webview_cef_plugin_c_api.h — Flutter runner 调用的 C API。
//
// 这是 Flutter Windows runner 嵌入 CEF 进程模型所需的最小入口。
// 在 wWinMain 中:
//   1. 调用 initCEFProcesses(instance) 让 CEF 分派子进程;
//   2. 在消息循环中调用 handleWndProcForCEF(...) 把 Windows 消息
//      转发给 CEF。

#ifndef WEBVIEW_CEF_PLUGIN_C_API_H_
#define WEBVIEW_CEF_PLUGIN_C_API_H_

#include <windows.h>

#ifdef __cplusplus
#include <flutter_windows.h>
extern "C" {
#else
typedef void* FlutterDesktopPluginRegistrarRef;
#endif

// 嵌入 CEF 进程模型。返回值 < 0 时表示当前是 browser 进程,继续初始化;
// 返回值 >= 0 表示当前是 CEF 子进程,立即以该值退出 wWinMain。
__declspec(dllexport) int initCEFProcesses(HINSTANCE hInstance);

// 把 Windows 消息转发给 CEF 内部。
__declspec(dllexport) void handleWndProcForCEF(HWND hwnd,
                                               unsigned int message,
                                               unsigned __int64 wParam,
                                               __int64 lParam);

// 注册插件到 Flutter 的 plugin registrar。
__declspec(dllexport) void WebviewCefPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar);

#ifdef __cplusplus
}
#endif

#endif  // WEBVIEW_CEF_PLUGIN_C_API_H_
