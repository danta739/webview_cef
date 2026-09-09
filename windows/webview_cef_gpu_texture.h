#ifndef WEBVIEW_CEF_GPU_TEXTURE_H
#define WEBVIEW_CEF_GPU_TEXTURE_H

#include "../common/webview_plugin.h"

#include <flutter_texture_registrar.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <mutex>

namespace webview_cef {
    // Zero-copy GPU renderer.
    //
    // CEF delivers each off-screen frame as a Direct3D 11 shared texture through
    // CefRenderHandler::OnAcceleratedPaint. That texture is owned by an internal
    // pool and is only valid for the duration of the callback, so we open it on
    // our own D3D11 device and CopyResource it into a "bridge" texture that we
    // own. The bridge texture is exposed to Flutter/ANGLE as a DXGI shared
    // handle (kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle); ANGLE opens that
    // handle on its own device, so the browser's GPU output reaches the Flutter
    // compositor without ever touching the CPU (no BGRA->RGBA swizzle, no
    // CPU->GPU upload).
    class WebviewGpuTextureRenderer : public WebviewTexture {
    public:
        explicit WebviewGpuTextureRenderer(FlutterDesktopTextureRegistrarRef registrar);
        ~WebviewGpuTextureRenderer() override;

        // True once a D3D11 device was created and the texture was registered.
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
        // Legacy DXGI share handle of |bridge_tex_| handed to Flutter/ANGLE.
        // Owned by the texture; not closed explicitly.
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
