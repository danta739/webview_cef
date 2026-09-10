#ifndef WEBVIEW_CEF_GPU_TEXTURE_H
#define WEBVIEW_CEF_GPU_TEXTURE_H

#include "../common/webview_plugin.h"

#include <flutter_texture_registrar.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <mutex>

namespace webview_cef {
    // 零拷贝 GPU 渲染器。
    //
    // CEF 通过 CefRenderHandler::OnAcceleratedPaint 将每个离屏帧作为 Direct3D 11
    // 共享纹理传递。该纹理由内部池拥有，仅在回调期间有效，因此我们在自己的 D3D11
    // 设备上打开它，并将其 CopyResource 到我们拥有的"桥接"纹理中。桥接纹理作为
    // DXGI 共享句柄 (kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle) 暴露给 Flutter/ANGLE；
    // ANGLE 在自己的设备上打开该句柄，因此浏览器的 GPU 输出无需经过 CPU 即可到达
    // Flutter 合成器（无需 BGRA->RGBA 转换，无需 CPU->GPU 上传）。
    class WebviewGpuTextureRenderer : public WebviewTexture {
    public:
        explicit WebviewGpuTextureRenderer(FlutterDesktopTextureRegistrarRef registrar);
        ~WebviewGpuTextureRenderer() override;

        // 一旦创建 D3D11 设备并注册纹理，返回 true。
        bool isValid() const { return device_ && textureId != 0; }

        void onAcceleratedFrame(const void* sharedHandle, int width, int height, int format) override;

    private:
        /// <summary>
        /// CEF 通过 CefRenderHandler::OnAcceleratedPaint 每帧交付一个离屏 D3D11 共享纹理，但该纹理仅在当前回调期间有效
        /// </summary>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        const FlutterDesktopGpuSurfaceDescriptor* ObtainDescriptor(size_t width, size_t height);
        bool EnsureBridgeTexture(UINT width, UINT height, DXGI_FORMAT format);
        ///flutter 纹理注册指针
        FlutterDesktopTextureRegistrarRef registrar_ = nullptr;
        ///D3D11设备
        Microsoft::WRL::ComPtr<ID3D11Device1> device_;
        ///D3D11设备上下文
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
        ///桥接纹理
        Microsoft::WRL::ComPtr<ID3D11Texture2D> bridge_tex_;
        // 传递给 Flutter/ANGLE 的 |bridge_tex_| 的旧版 DXGI 共享句柄。
        // 由纹理拥有；不显式关闭。
        ///桥接纹理的共享句柄
        HANDLE shared_handle_ = nullptr;
        ///当前桥接纹理的宽高
        UINT tex_width_ = 0;
        UINT tex_height_ = 0;
        ///当前桥接纹理的DXGI格式

        DXGI_FORMAT tex_format_ = DXGI_FORMAT_B8G8R8A8_UNORM;
        /// flutter 的纹理格式
        FlutterDesktopPixelFormat flutter_format_ = kFlutterDesktopPixelFormatBGRA8888;
        std::mutex mutex_;
    };
}

#endif  // WEBVIEW_CEF_GPU_TEXTURE_H
