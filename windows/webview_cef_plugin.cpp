#include "webview_cef_plugin.h"
#include "webview_cef_keyevent.h"
#ifdef WEBVIEW_CEF_GPU_TEXTURE
#include "webview_cef_gpu_texture.h"
#endif
// 必须在许多其他 Windows 头文件之前包含。
#include <windows.h>
#include <imm.h>
#include <commctrl.h>

// 用于 getPlatformVersion；除非插件实现需要，否则请删除。
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter_windows.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <thread>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

namespace webview_cef {
	WebviewTextureRenderer::WebviewTextureRenderer(
		FlutterDesktopTextureRegistrarRef texture_registrar)
		: registrar_(texture_registrar) {
		texture = std::make_unique<flutter::TextureVariant>(
			flutter::PixelBufferTexture([this](size_t width, size_t height) {
				return this->CopyPixelBuffer(width, height);
			}));
		FlutterDesktopTextureInfo info = {};
		info.type = kFlutterDesktopPixelBufferTexture;
		info.pixel_buffer_config.user_data =
			std::get_if<flutter::PixelBufferTexture>(texture.get());
		info.pixel_buffer_config.callback =
			[](size_t width, size_t height, void* user_data) {
				auto texture = static_cast<flutter::PixelBufferTexture*>(user_data);
				return texture->CopyPixelBuffer(width, height);
			};
		textureId = FlutterDesktopTextureRegistrarRegisterExternalTexture(
			registrar_, &info);
	}

	WebviewTextureRenderer::~WebviewTextureRenderer() {
		std::lock_guard<std::mutex> autolock(mutex_);
		if (registrar_) {
			// FlutterDesktopTextureRegistrarUnregisterExternalTexture(registrar_, textureId, nullptr, nullptr);
		}
	}

	const FlutterDesktopPixelBuffer* WebviewTextureRenderer::CopyPixelBuffer(
		size_t width, size_t height) const {
		std::lock_guard<std::mutex> autolock(mutex_);
		return pixel_buffer.get();
	}

	void WebviewTextureRenderer::onFrame(const void* buffer, int width, int height) {
		const std::lock_guard<std::mutex> autolock(mutex_);
		if (!pixel_buffer.get() || pixel_buffer.get()->width != width ||
			pixel_buffer.get()->height != height) {
			if (!pixel_buffer.get()) {
				pixel_buffer = std::make_unique<FlutterDesktopPixelBuffer>();
				pixel_buffer->release_context = nullptr;
			}
			pixel_buffer->width = width;
			pixel_buffer->height = height;
			const auto size = width * height * 4;
			backing_pixel_buffer.reset(new uint8_t[size]);
			pixel_buffer->buffer = backing_pixel_buffer.get();
		}

		SwapBufferFromBgraToRgba((void*)pixel_buffer->buffer, buffer, width, height);
		if (registrar_) {
			FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(
				registrar_, textureId);
		}
	}

	static flutter::EncodableValue encode_wvalue_to_flvalue(WValue* args) {
		// 空值（或 Null 类型的 WValue）映射到 null EncodableValue。
		// 注意：不能使用 EncodableValue(nullptr) — 在 C++20 下它解析为
		// std::string(const char*=nullptr)，会导致 strlen 崩溃。
		if (args == nullptr) {
			return flutter::EncodableValue();
		}
		WValueType type = webview_value_get_type(args);
		switch(type){
			case Webview_Value_Type_Bool:
				return flutter::EncodableValue(webview_value_get_bool(args));
			case Webview_Value_Type_Int:
				return flutter::EncodableValue(webview_value_get_int(args));
			case Webview_Value_Type_Float:
				// flutter::EncodableValue 没有标量 float 替代类型；C++20 更严格的
				// std::variant 规则不再自动将 float 提升为 double。
				return flutter::EncodableValue(static_cast<double>(webview_value_get_float(args)));
			case Webview_Value_Type_Double:
				return flutter::EncodableValue(webview_value_get_double(args));
			case Webview_Value_Type_String:
				return flutter::EncodableValue(webview_value_get_string(args));
			case Webview_Value_Type_Uint8_List: {
				const uint8_t* data = webview_value_get_uint8_list(args);
				return flutter::EncodableValue(std::vector<uint8_t>(data, data + webview_value_get_len(args)));
			}
			case Webview_Value_Type_Int32_List: {
				const int32_t* data = webview_value_get_int32_list(args);
				return flutter::EncodableValue(std::vector<int32_t>(data, data + webview_value_get_len(args)));
			}
			case Webview_Value_Type_Int64_List: {
				const int64_t* data = webview_value_get_int64_list(args);
				return flutter::EncodableValue(std::vector<int64_t>(data, data + webview_value_get_len(args)));
			}
			case Webview_Value_Type_Float_List: {
				const float* data = webview_value_get_float_list(args);
				return flutter::EncodableValue(std::vector<float>(data, data + webview_value_get_len(args)));
			}
			case Webview_Value_Type_Double_List: {
				const double* data = webview_value_get_double_list(args);
				return flutter::EncodableValue(std::vector<double>(data, data + webview_value_get_len(args)));
			}
			case Webview_Value_Type_List:
			{
				flutter::EncodableList ret;
				size_t len = webview_value_get_len(args);
				for (size_t i = 0; i < len; i++) {
                	ret.push_back(encode_wvalue_to_flvalue(webview_value_get_list_value(args, i)));
				}
				return ret;
			}
			case Webview_Value_Type_Map:
			{
				flutter::EncodableMap ret;
				size_t len = webview_value_get_len(args);
				for (size_t i = 0; i < len; i++) {
					ret[encode_wvalue_to_flvalue(webview_value_get_key(args, i))] = encode_wvalue_to_flvalue(webview_value_get_value(args, i));
				}
				return ret;
			}
			default:
				return flutter::EncodableValue();
		}
	}

	static WValue *encode_flvalue_to_wvalue(flutter::EncodableValue* args) {
		size_t index = args->index();
		if (index == 1) {
			return webview_value_new_bool(*std::get_if<bool>(args));
		}
		else if (index == 2 || index == 3) {
			return webview_value_new_int(*std::get_if<int32_t>(args));
		}
		else if (index == 4) {
			return webview_value_new_double(*std::get_if<double>(args));
		}
		else if (index == 5) {
			return webview_value_new_string((*std::get_if<std::string>(args)).c_str());
		}
		else if (index == 6) {
			auto list = *std::get_if<std::vector<uint8_t>>(args);
			return webview_value_new_uint8_list(list.data(), list.size());
		}
		else if (index == 7) {
			auto list = *std::get_if<std::vector<int32_t>>(args);
			return webview_value_new_int32_list(list.data(), list.size());
		}
		else if (index == 8) {
			auto list = *std::get_if<std::vector<int64_t>>(args);
			return webview_value_new_int64_list(list.data(), list.size());
		}
		else if (index == 9) {
			auto list = *std::get_if<std::vector<double>>(args);
			return webview_value_new_double_list(list.data(), list.size());
		}
		else if (index == 10) {
			WValue * ret = webview_value_new_list();
			flutter::EncodableList list = *std::get_if<flutter::EncodableList>(args);
			for (size_t i = 0; i < list.size(); i++) {
				WValue *value = encode_flvalue_to_wvalue(&list[i]);
				webview_value_append(ret, value);
				webview_value_unref(value);
			}
			return ret;
		}
		else if (index == 11) {
			WValue * ret = webview_value_new_map();
			flutter::EncodableMap map = *std::get_if<flutter::EncodableMap>(args);
			for (flutter::EncodableMap::iterator it = map.begin(); it != map.end(); it++)
			{
				WValue *key = encode_flvalue_to_wvalue(const_cast<flutter::EncodableValue *>(&it->first));
				WValue *value = encode_flvalue_to_wvalue(const_cast<flutter::EncodableValue*>(&it->second));
				webview_value_set(ret, key, value);
				webview_value_unref(key);
				webview_value_unref(value);
			}
			return ret;
		}
		else if (index == 12) {
			return nullptr;
		}
		else if (index == 13) {
			auto list = *std::get_if<std::vector<float>>(args);
			return webview_value_new_float_list(list.data(), list.size());
		}
		return nullptr;
	}

	std::unordered_map<HWND, std::shared_ptr<WebviewPlugin>> webviewPlugins;
	std::unordered_map<HWND, std::function<void(std::string method,flutter::EncodableValue * arguments)>> webviewChannels;
	// 防止 webviewPlugins 被 vsync 驱动线程的快照读取访问。
	static std::mutex g_pluginsMutex;

#ifdef WEBVIEW_CEF_GPU_TEXTURE
	// ---- Vsync 驱动的外部 BeginFrame -----------------------------------
	// 启用 external_begin_frame_enabled 后，浏览器仅在调用 SendExternalBeginFrame 时
	// 才生成一帧。我们在每个显示 vblank 上触发它，使 webview 的帧率跟随显示器的
	// 实际刷新率（自适应，不受 60 限制）。WaitForVBlank 阻塞至下一个 vblank，
	// 并自动跟踪当前刷新率。
	static std::thread g_vsyncThread;
	static std::atomic<bool> g_vsyncRunning{false};

	static Microsoft::WRL::ComPtr<IDXGIOutput> AcquirePrimaryDxgiOutput() {
		Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return nullptr;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		if (FAILED(factory->EnumAdapters1(0, &adapter))) return nullptr;
		Microsoft::WRL::ComPtr<IDXGIOutput> output;
		if (FAILED(adapter->EnumOutputs(0, &output))) return nullptr;
		return output;
	}

	static UINT QueryRefreshIntervalMs() {
		DEVMODE dm = {};
		dm.dmSize = sizeof(dm);
		if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1) {
			return (std::max)(UINT(1), UINT(1000 / dm.dmDisplayFrequency));
		}
		return 16;  // assume ~60 Hz
	}

	static void VsyncThreadProc() {
		Microsoft::WRL::ComPtr<IDXGIOutput> output = AcquirePrimaryDxgiOutput();
		const UINT fallbackMs = QueryRefreshIntervalMs();
		while (g_vsyncRunning) {
			if (output) {
				if (FAILED(output->WaitForVBlank())) {
					output.Reset();
				}
			}
			if (!output) {
				std::this_thread::sleep_for(std::chrono::milliseconds(fallbackMs));
				output = AcquirePrimaryDxgiOutput();
			}
			std::vector<std::shared_ptr<WebviewPlugin>> snapshot;
			{
				std::lock_guard<std::mutex> lock(g_pluginsMutex);
				snapshot.reserve(webviewPlugins.size());
				for (auto& kv : webviewPlugins) snapshot.push_back(kv.second);
			}
			for (auto& p : snapshot) {
				if (p) p->tickBeginFrame();
			}
		}
	}

	static void StartVsyncDriver() {
		bool expected = false;
		if (g_vsyncRunning.compare_exchange_strong(expected, true)) {
			g_vsyncThread = std::thread(VsyncThreadProc);
		}
	}

	static void StopVsyncDriver() {
		if (g_vsyncRunning.exchange(false)) {
			if (g_vsyncThread.joinable()) g_vsyncThread.join();
		}
	}
#endif  // WEBVIEW_CEF_GPU_TEXTURE

	static constexpr UINT_PTR kImeSubclassId = 1;

	// 操作系统 IME 必须在 DefWindowProc 运行之前的窗口过程中拦截，
	// 否则默认处理会消耗/转换结果字符串并显示自己的组合窗口。
	// 我们无法从 runner 的 post-DispatchMessage 钩子中执行此操作，
	// 因此在这里对 Flutter 视图 HWND 进行子类化。
	static LRESULT CALLBACK ImeSubclassProc(HWND hwnd, UINT message, WPARAM wparam,
		LPARAM lparam, UINT_PTR, DWORD_PTR) {
		switch (message) {
		case WM_IME_SETCONTEXT:
			// 不让操作系统绘制自己的组合窗口；预编辑通过 ImeSetComposition
			// 在网页内部渲染。
			lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
			return DefSubclassProc(hwnd, message, wparam, lparam);
		case WM_IME_STARTCOMPOSITION:
			// 禁止默认的组合 UI；我们自己驱动组合。
			return 0;
		case WM_IME_COMPOSITION: {
			auto pit = webviewPlugins.find(hwnd);
			if (pit == webviewPlugins.end() || !pit->second->isEditableFocused()) {
				return DefSubclassProc(hwnd, message, wparam, lparam);
			}
			HIMC imc = ImmGetContext(hwnd);
			if (imc) {
				// 已提交结果 -> 提交到焦点浏览器。
				if (lparam & GCS_RESULTSTR) {
					LONG bytes = ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
					if (bytes > 0) {
						std::wstring ws(bytes / sizeof(wchar_t), L'\0');
						ImmGetCompositionStringW(imc, GCS_RESULTSTR, &ws[0], bytes);
						pit->second->imeCommitTextNative(ws);
					}
				}
				// 正在进行的预编辑 -> 设置组合（实时屏幕文本）。
				if (lparam & GCS_COMPSTR) {
					LONG bytes = ImmGetCompositionStringW(imc, GCS_COMPSTR, nullptr, 0);
					if (bytes > 0) {
						std::wstring ws(bytes / sizeof(wchar_t), L'\0');
						ImmGetCompositionStringW(imc, GCS_COMPSTR, &ws[0], bytes);
						int cursor = static_cast<int>(
							ImmGetCompositionStringW(imc, GCS_CURSORPOS, nullptr, 0));
						pit->second->imeSetCompositionNative(ws, cursor);
					}
				}
				ImmReleaseContext(hwnd, imc);
			}
			// 消费：防止 DefWindowProc 显示默认 IME 窗口或为已提交文本
			// 生成重复的 WM_IME_CHAR/WM_CHAR。
			return 0;
		}
		case WM_IME_ENDCOMPOSITION: {
			auto pit = webviewPlugins.find(hwnd);
			if (pit != webviewPlugins.end()) {
				pit->second->imeFinishCompositionNative();
			}
			return DefSubclassProc(hwnd, message, wparam, lparam);
		}
		}
		return DefSubclassProc(hwnd, message, wparam, lparam);
	}

	void WebviewCefPlugin::RegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {

		auto plugin = std::make_unique<WebviewCefPlugin>();
		plugin->m_textureRegistrar = FlutterDesktopRegistrarGetTextureRegistrar(registrar);
		flutter::PluginRegistrarWindows *window_registrar = flutter::PluginRegistrarManager::GetInstance()
																->GetRegistrar<flutter::PluginRegistrarWindows>(registrar);
		plugin->m_channel =
			std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
				window_registrar->messenger(), "webview_cef",
				&flutter::StandardMethodCodec::GetInstance());

		plugin->m_channel->SetMethodCallHandler(
			[plugin_pointer = plugin.get()](const auto &call, auto result)
			{
				plugin_pointer->HandleMethodCall(call, std::move(result));
			});

		plugin->m_hwnd = FlutterDesktopViewGetHWND(FlutterDesktopPluginRegistrarGetView(registrar));
		{
			std::lock_guard<std::mutex> lock(g_pluginsMutex);
			webviewPlugins.emplace(plugin->m_hwnd, plugin->m_plugin);
		}
#ifdef WEBVIEW_CEF_GPU_TEXTURE
		// 在每个 vblank 上开始触发外部 BeginFrame（驱动 GPU 帧）。
		StartVsyncDriver();
#endif
		// 子类化 Flutter 视图窗口，在引擎和 DefWindowProc 处理 WM_IME_* 之前拦截
		// （正确处理 CJK 组合/提交所必需）。
		SetWindowSubclass(plugin->m_hwnd, ImeSubclassProc, kImeSubclassId, 0);
		webviewChannels.emplace(plugin->m_hwnd, [plugin_pointer = plugin.get()](std::string method, flutter::EncodableValue* arguments) {
			plugin_pointer->m_channel->InvokeMethod(method, std::make_unique<flutter::EncodableValue>(*arguments));
			});
		plugin->m_plugin->setInvokeMethodFunc([plugin_pointer = plugin.get()](std::string method, WValue* arguments) {
			flutter::EncodableValue* methodValue = new flutter::EncodableValue(method);
			flutter::EncodableValue* args = new flutter::EncodableValue(encode_wvalue_to_flvalue(arguments));
			PostMessage(plugin_pointer->m_hwnd, WM_USER + 1, WPARAM(methodValue), LPARAM(args));
			});

		plugin->m_plugin->setCreateTextureFunc([plugin_pointer = plugin.get()]() {
#ifdef WEBVIEW_CEF_GPU_TEXTURE
			// 零拷贝 GPU 路径：CEF OnAcceleratedPaint 共享纹理 -> Flutter
			// D3D11 表面纹理。仅在无法创建 D3D11 设备时才回退到软件像素缓冲区路径。
			///判断是否可以创建GPU句柄
			auto gpu = std::make_shared<WebviewGpuTextureRenderer>(plugin_pointer->m_textureRegistrar);
			if (gpu->isValid()) {
				return std::dynamic_pointer_cast<WebviewTexture>(gpu);
			}
#endif
			std::shared_ptr<WebviewTextureRenderer> renderer = std::make_shared<WebviewTextureRenderer>(plugin_pointer->m_textureRegistrar);
			return std::dynamic_pointer_cast<WebviewTexture>(renderer);
		});

		window_registrar->AddPlugin(std::move(plugin));
	}
		
	WebviewCefPlugin::WebviewCefPlugin() {
		m_plugin = std::make_shared<WebviewPlugin>();
	}

	WebviewCefPlugin::~WebviewCefPlugin() {
        m_plugin = nullptr;
		RemoveWindowSubclass(m_hwnd, ImeSubclassProc, kImeSubclassId);
		bool nowEmpty = false;
		{
			std::lock_guard<std::mutex> lock(g_pluginsMutex);
			webviewPlugins.erase(m_hwnd);
			nowEmpty = webviewPlugins.empty();
		}
		webviewChannels.erase(m_hwnd);
        if(nowEmpty){
#ifdef WEBVIEW_CEF_GPU_TEXTURE
			// 在关闭 CEF 之前停止 vsync 线程。这里不能持有 g_pluginsMutex：
			// 线程在快照时需要它。
			StopVsyncDriver();
#endif
			webview_cef::stopCEF();
		}
	}
/// @brief 处理 Flutter MethodChannel 的方法调用
/// @param method_call 方法名 + 参数（EncodableValue 格式）
/// @param result      结果回调，用于将处理结果返回给 Flutter 端
	void WebviewCefPlugin::HandleMethodCall(
		const flutter::MethodCall<flutter::EncodableValue>& method_call,
		std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
		WValue *encodeArgs = encode_flvalue_to_wvalue(const_cast<flutter::EncodableValue *>(method_call.arguments()));
		m_plugin->HandleMethodCall(method_call.method_name(), encodeArgs, [=](int ret, WValue* args){
			if (ret > 0){
				result->Success(encode_wvalue_to_flvalue(args));
			}
			else if (ret < 0){
				result->Error("error", "error", encode_wvalue_to_flvalue(args));
			}
			else{
				result->NotImplemented();
			}
		});
		webview_value_unref(encodeArgs);
	}

	void WebviewCefPlugin::handleMessageProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
		switch (message) {
		case WM_USER + 1:
		{
			// 这些在 setInvokeMethodFunc 中通过堆分配；在分发后获取所有权并释放
			// （InvokeMethod 会复制参数）。
			flutter::EncodableValue *method = (flutter::EncodableValue *)wparam;
			flutter::EncodableValue *args = (flutter::EncodableValue *)lparam;
			if (webviewPlugins.find(hwnd) != webviewPlugins.end()) {
				webviewChannels[hwnd]((*std::get_if<std::string>(method)), args);
			}
			delete method;
			delete args;
			break;
		}
		// WM_IME_* 在 ImeSubclassProc 中处理（早于 DefWindowProc），而不是此处。
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR: {
			if (webviewPlugins.find(hwnd) != webviewPlugins.end()) {
				CefKeyEvent keyEvent = getCefKeyEvent(message, wparam, lparam);
				webviewPlugins[hwnd]->sendKeyEvent(keyEvent);
			}
			break;
		}
		}
	}

}  // namespace webview_cef
