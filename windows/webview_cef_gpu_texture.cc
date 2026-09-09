#include "webview_cef_gpu_texture.h"

#include <flutter_windows.h>

#include <iostream>

namespace webview_cef {

    using Microsoft::WRL::ComPtr;

    namespace {
        ///像素格式映射
        // Maps CEF/D3D BGRA or RGBA to the matching Flutter pixel format.
        FlutterDesktopPixelFormat ToFlutterFormat(DXGI_FORMAT format) {
            switch (format) {
                case DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                    return kFlutterDesktopPixelFormatRGBA8888;
                default:
                    return kFlutterDesktopPixelFormatBGRA8888;
            }
        }

        // Holds a reference to the bridge texture for the lifetime of one Flutter
        // "obtain descriptor" call so the shared handle stays valid until Flutter
        // has opened it. The engine invokes release_callback when it is done.
        struct   {
            ComPtr<ID3D11Texture2D> texture;
            FlutterDesktopGpuSurfaceDescriptor descriptor = {};
        };
    }  // namespace
    // 构造函数：接收 Flutter 的纹理注册器引用（registrar），用于后续向 Flutter 注册外部纹理。
    WebviewGpuTextureRenderer::WebviewGpuTextureRenderer(FlutterDesktopTextureRegistrarRef registrar)
        : registrar_(registrar) {

        // 设置设备创建标志：必须包含 BGRA_SUPPORT。
        // Create our own hardware D3D11 device. BGRA support is required so the
        // bridge texture format matches CEF's output and Flutter/ANGLE.
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        ///期望的 Direct3D 功能级别
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ///  // 尝试创建硬件加速的 D3D11 设备。
        // 参数说明：
        // - nullptr: 使用默认显示适配器（显卡）
        // - D3D_DRIVER_TYPE_HARDWARE: 使用硬件光栅化器（即真实显卡）
        // - nullptr: 不使用软件光栅化 DLL
        // - flags: 传入前面定义的 BGRA_SUPPORT
        // - levels, ARRAYSIZE(levels): 传入功能级别数组及其长度
        // - D3D11_SDK_VERSION: 当前 DirectX SDK 版本
        // - &device, nullptr, &context: 输出参数，分别接收设备指针
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                       levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                       &device, nullptr, &context);
        ///如果硬件设备创建失败，则回退到 WARP （CPU）软件渲染器。
        if (FAILED(hr)) {
            // Fall back to the WARP software renderer (still GPU-surface based).
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                                   levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   &device, nullptr, &context);
        }
        if (FAILED(hr)) {
            std::cerr << "webview_cef: D3D11CreateDevice failed (0x" << std::hex << hr
                      << "); GPU texture path unavailable." << std::endl;
            return;
        }
        ///CEF传来的共享纹理使用的是 NT句柄，必须先调用OpenSharedResource1 
        // 而基础的 ID3D11Device 没有此方法。因此需要通过 COM 的 QueryInterface 机制
        // 将设备向上转型（As）为 ID3D11Device1。
        // OpenSharedResource1 (for CEF's NT shared handle) needs ID3D11Device1.
        if (FAILED(device.As(&device_))) {
            device_.Reset();
            return;
        }
        context_ = context;

        ///向 Flutter 注册外部 GPU 纹理
        // 初始化 Flutter 外部纹理信息结构体。
        FlutterDesktopTextureInfo info = {};
        ///设置纹理类型为GPU纹理，纹理由GPU直接管理，不通过CPU
        info.type = kFlutterDesktopGpuSurfaceTexture;
        info.gpu_surface_config.struct_size = sizeof(FlutterDesktopGpuSurfaceTextureConfig);
        // 共享机制为 DXGI Shared Handle，提供一个 Windows 平台的 DXGI 共享句柄给 Flutter，
        // ANGLE 会在自己的 D3D 设备上打开这个句柄，从而实现真正的零拷贝跨进程/跨设备渲染。
        info.gpu_surface_config.type = kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle;
        info.gpu_surface_config.user_data = this;

        ///当 Flutter 引擎需要获取当前帧的纹理描述符（包含共享句柄、宽高、格式等）时，
        // 此回调函数将 C 风格的回调桥接到 C++ 成员函数 ObtainDescriptor。
        info.gpu_surface_config.callback =
            [](size_t width, size_t height, void* user_data) -> const FlutterDesktopGpuSurfaceDescriptor* {
                return static_cast<WebviewGpuTextureRenderer*>(user_data)->ObtainDescriptor(width, height);
            };
        /// 调用 Flutter C API，将上述配置注册到 Flutter 引擎中。
        // 引擎会返回一个唯一的 textureId，后续在 Dart 层可以通过此 ID 来引用和显示该纹理。
        textureId = FlutterDesktopTextureRegistrarRegisterExternalTexture(registrar_, &info);
    }

    WebviewGpuTextureRenderer::~WebviewGpuTextureRenderer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (registrar_ && textureId) {
            FlutterDesktopTextureRegistrarUnregisterExternalTexture(registrar_, textureId, nullptr, nullptr);
        }
    }
    /// <summary>
    /// 确保桥接宽高和格式合法 
    /// </summary>
    /// <param name="width"></param>
    /// <param name="height"></param>
    /// <param name="format"></param>
    /// <returns></returns>
    bool WebviewGpuTextureRenderer::EnsureBridgeTexture(UINT width, UINT height, DXGI_FORMAT format) {
        if (bridge_tex_ && tex_width_ == width && tex_height_ == height && tex_format_ == format) {
            return true;
        }
        bridge_tex_.Reset();
        shared_handle_ = nullptr;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        ///把纹理生成一个共享资源的句柄（GetSharedHandle
        // Legacy shared so ANGLE can open the texture on its own device through a
        // share handle (EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE).
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &bridge_tex_))) {
            return false;
        }
        ComPtr<IDXGIResource> dxgi_resource;
        if (FAILED(bridge_tex_.As(&dxgi_resource)) ||
            FAILED(dxgi_resource->GetSharedHandle(&shared_handle_)) || !shared_handle_) {
            bridge_tex_.Reset();
            shared_handle_ = nullptr;
            return false;
        }
        tex_width_ = width;
        tex_height_ = height;
        tex_format_ = format;
        flutter_format_ = ToFlutterFormat(format);
        return true;
    }

    void WebviewGpuTextureRenderer::onAcceleratedFrame(const void* sharedHandle, int width, int height, int format) {
        if (!device_ || sharedHandle == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);

        // CEF's shared texture is pool-owned and only valid during this call, so
        // reopen it on our device every frame (NT handle -> OpenSharedResource1).
        ///将CEF 传来的共享句柄还原为本设备上的 ID3D11Texture2D 对象
        ComPtr<ID3D11Texture2D> cef_tex;
        HRESULT hr = device_->OpenSharedResource1(
            reinterpret_cast<HANDLE>(const_cast<void*>(sharedHandle)), IID_PPV_ARGS(&cef_tex));
        if (FAILED(hr) || !cef_tex) {
            return;
        }

        D3D11_TEXTURE2D_DESC src_desc = {};
        cef_tex->GetDesc(&src_desc);
        ///获取源纹理描述符,确保桥接纹理尺寸/格式匹配
        if (!EnsureBridgeTexture(src_desc.Width, src_desc.Height, src_desc.Format)) {
            return;
        }
        ///GPU零拷贝
        context_->CopyResource(bridge_tex_.Get(), cef_tex.Get());
        ///确保命令执行完毕
        context_->Flush();
        ///通知Flutter 有新帧可绘制
        if (registrar_ && textureId) {
            FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(registrar_, textureId);
        }
    }
    /// <summary>
    /// 响应 Flutter 引擎的纹理描述符请求。
    /// 当 Flutter 光栅线程准备绘制外部纹理时，会调用此方法获取当前帧的 GPU 表面信息。
    /// </summary>
    /// <param name="width">Flutter 期望的纹理像素宽度</param>
    /// <param name="height">Flutter 期望的纹理像素高度</param>
    /// <returns>包含共享纹理句柄、尺寸、格式及释放回调的描述符指针；若纹理未就绪则返回 nullptr,画面空白或暂停上一帧</returns>
    const FlutterDesktopGpuSurfaceDescriptor* WebviewGpuTextureRenderer::ObtainDescriptor(size_t width, size_t height) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!bridge_tex_ || !shared_handle_) {
            return nullptr;
        }
        //DescriptorHolder 通过在堆上分配内存，并将指针交给 Flutter，同时配合 release_callback 告诉 Flutter,当用完了这个 Descriptor，调用这个回调函数，会自己把这块内存 delete 掉。
        auto* holder = new DescriptorHolder();
        holder->texture = bridge_tex_;  // keep alive until Flutter opens the handle
        holder->descriptor.struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
        holder->descriptor.handle = shared_handle_;
        holder->descriptor.width = holder->descriptor.visible_width = tex_width_;
        holder->descriptor.height = holder->descriptor.visible_height = tex_height_;
        holder->descriptor.format = flutter_format_;
        holder->descriptor.release_callback = [](void* release_context) {
            delete static_cast<DescriptorHolder*>(release_context);
        };
        holder->descriptor.release_context = holder;
        return &holder->descriptor;
    }
}  // namespace webview_cef
