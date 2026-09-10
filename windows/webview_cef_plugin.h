#ifndef FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_H_
#define FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/texture_registrar.h>

#include <webview_plugin.h>
#include <memory>
#include <mutex>

namespace webview_cef {

class WebviewTextureRenderer : public WebviewTexture {
 public:
  explicit WebviewTextureRenderer(
      FlutterDesktopTextureRegistrarRef texture_registrar);
  ~WebviewTextureRenderer() override;

  const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width,
                                                     size_t height) const;
  void onFrame(const void* buffer, int width, int height) override;

 private:
  FlutterDesktopTextureRegistrarRef registrar_ = nullptr;
  std::unique_ptr<flutter::TextureVariant> texture;
  mutable std::shared_ptr<FlutterDesktopPixelBuffer> pixel_buffer;
  std::unique_ptr<uint8_t> backing_pixel_buffer;
  mutable std::mutex mutex_;
};

class WebviewCefPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar);
  static void handleMessageProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  WebviewCefPlugin();
  virtual ~WebviewCefPlugin();

  // 禁止拷贝和赋值。
  WebviewCefPlugin(const WebviewCefPlugin&) = delete;
  WebviewCefPlugin& operator=(const WebviewCefPlugin&) = delete;

 private:
  // 当 Dart 端调用此插件通道的方法时调用。
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  std::shared_ptr<WebviewPlugin> m_plugin;
  
	FlutterDesktopTextureRegistrarRef m_textureRegistrar;

	std::unique_ptr<
		flutter::MethodChannel<flutter::EncodableValue>,
		std::default_delete<flutter::MethodChannel<flutter::EncodableValue>>>
		m_channel = nullptr;

  DWORD m_mainThreadId;
  HWND m_hwnd;
};

}  // namespace webview_cef

#endif  // FLUTTER_PLUGIN_WEBVIEW_CEF_PLUGIN_H_
