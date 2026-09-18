// webview_app.cc — WebviewApp 实现。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_app.h"

#include <string>

#include "include/cef_command_line.h"

namespace webview_cef {

void WebviewApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    // 默认开启 GPU、自动播放音视频、白名单化一些 CEF 开关。
    if (process_type.empty()) {
        command_line->AppendSwitchWithValue("enable-features",
                                           "UseChromeOSDirectVideoDecoder");
    }
    // 关闭同源策略限制(便于嵌入本地文件与远程 URL 共存)。
    command_line->AppendSwitch("disable-web-security");
    // 允许文件协议访问跨域资源。
    command_line->AppendSwitch("allow-file-access-from-files");
    // 暴露更多控制台日志。
    command_line->AppendSwitch("enable-logging");
    command_line->AppendSwitchWithValue("v", "1");
}

void WebviewApp::OnContextInitialized() {
    // 暂时无需额外动作。
}

}  // namespace webview_cef
