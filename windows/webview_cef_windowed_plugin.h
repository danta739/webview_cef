// webview_cef_windowed_plugin.h — Flutter 端屏上渲染模式的桥接层。

#ifndef WEBVIEW_CEF_WINDOWED_PLUGIN_H_
#define WEBVIEW_CEF_WINDOWED_PLUGIN_H_

#include <flutter/flutter_view.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <windows.h>

#include <memory>
#include <string>

#include "webview_windowed.h"

namespace webview_cef {

class WindowedWebviewPlugin {
 public:
    WindowedWebviewPlugin(flutter::PluginRegistrar* registrar,
                          HWND parent_hwnd);
    ~WindowedWebviewPlugin();

    void BindChannel();
    void Shutdown();

    void SendEvent(const std::string& name, flutter::EncodableValue payload);

 private:
    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue>& call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    flutter::PluginRegistrar* registrar_;
    HWND parent_hwnd_;
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
    int active_browser_id_ = -1;
};

}  // namespace webview_cef

#endif  // WEBVIEW_CEF_WINDOWED_PLUGIN_H_
