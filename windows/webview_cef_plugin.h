// webview_cef_plugin.h — OSR Texture 渲染 + MethodChannel 绑定。

#ifndef WEBVIEW_CEF_PLUGIN_H_
#define WEBVIEW_CEF_PLUGIN_H_

#include <flutter/flutter_view.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/texture_registrar.h>

#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "webview_plugin.h"

namespace webview_cef {

class WebviewTextureRenderer : public WebviewTexture {
 public:
    explicit WebviewTextureRenderer(FlutterDesktopTextureRegistrarRef texture_registrar);
    ~WebviewTextureRenderer() override;

    void onFrame(const void* buffer, int width, int height) override;
    void onAcceleratedFrame(const void* sharedHandle,
                            int width, int height, int format) override;

    int64_t textureId = 0;

 private:
    const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width, size_t height) const;

    FlutterDesktopTextureRegistrarRef registrar_ = nullptr;
    std::unique_ptr<flutter::TextureVariant> texture;
    mutable std::mutex mutex_;
    std::unique_ptr<FlutterDesktopPixelBuffer> pixel_buffer;
    std::unique_ptr<uint8_t[]> backing_pixel_buffer;
};

class WebviewCefPlugin : public flutter::Plugin {
 public:
    static void RegisterWithRegistrar(flutter::PluginRegistrar* registrar);

    explicit WebviewCefPlugin(flutter::PluginRegistrar* registrar);
    ~WebviewCefPlugin() override;

    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue>& call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    void SendEvent(const std::string& name, flutter::EncodableValue payload);

    static LRESULT CALLBACK handleMessageProc(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam);
    void HandleImeMessage(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);

 private:
    void EnsurePlugin();
    void SetupBindings();
    void OnPaintCallback(int browserId, const void* buffer, int width, int height);
    void OnAcceleratedPaintCallback(int browserId, const void* sharedHandle,
                                    int width, int height, int format);

    flutter::PluginRegistrar* m_registrar = nullptr;
    HWND m_hwnd = nullptr;
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> m_channel;
    std::unique_ptr<WebviewPlugin> m_plugin;
    std::unordered_map<int, std::shared_ptr<WebviewTextureRenderer>> m_renderers;
    std::mutex m_mutex;
    std::atomic<bool> m_attached{false};
    WNDPROC m_orig_wndproc = nullptr;
};

}  // namespace webview_cef

#endif  // WEBVIEW_CEF_PLUGIN_H_
