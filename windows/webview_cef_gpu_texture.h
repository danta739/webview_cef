// webview_cef_gpu_texture.h — GPU 共享纹理接入点(可选)。

#ifndef WEBVIEW_CEF_GPU_TEXTURE_H_
#define WEBVIEW_CEF_GPU_TEXTURE_H_

#include <cstdint>

namespace webview_cef {

// 接入点:目前仅做桩实现,真实 GPU 纹理共享需要 ANGLE/D3D11 互操作。
struct GpuTextureFrame {
    void* shared_handle = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t format = 0;
};

inline void ProcessGpuFrame(const GpuTextureFrame&) {}

}  // namespace webview_cef

#endif  // WEBVIEW_CEF_GPU_TEXTURE_H_
