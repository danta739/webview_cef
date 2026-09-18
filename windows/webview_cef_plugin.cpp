// webview_cef_plugin.cpp — OSR Texture + MethodChannel 实现。

#include "webview_cef_plugin.h"

#include "webview_cef_keyevent.h"
#ifdef WEBVIEW_CEF_GPU_TEXTURE
#include "webview_cef_gpu_texture.h"
#endif

#include <windows.h>
#include <imm.h>
#include <commctrl.h>
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter_windows.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace webview_cef {

// ---------------- WebviewTextureRenderer ----------------

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
            auto* tex = static_cast<flutter::PixelBufferTexture*>(user_data);
            return tex->CopyPixelBuffer(width, height);
        };
    textureId = FlutterDesktopTextureRegistrarRegisterExternalTexture(
        registrar_, &info);
}

WebviewTextureRenderer::~WebviewTextureRenderer() {
    std::lock_guard<std::mutex> autolock(mutex_);
    if (registrar_ && textureId != 0) {
        // FlutterDesktopTextureRegistrarUnregisterExternalTexture(registrar_, textureId, nullptr, nullptr);
    }
}

const FlutterDesktopPixelBuffer* WebviewTextureRenderer::CopyPixelBuffer(
    size_t width, size_t height) const {
    std::lock_guard<std::mutex> autolock(mutex_);
    return pixel_buffer.get();
}

void WebviewTextureRenderer::onFrame(const void* buffer, int width, int height) {
    std::lock_guard<std::mutex> autolock(mutex_);
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
    SwapBufferFromBgraToRgba(const_cast<uint8_t*>(pixel_buffer->buffer), buffer, width, height);
    if (registrar_) {
        FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(
            registrar_, textureId);
    }
}

void WebviewTextureRenderer::onAcceleratedFrame(const void* sharedHandle,
                                                int width, int height, int format) {
    (void)sharedHandle; (void)format;
    std::lock_guard<std::mutex> autolock(mutex_);
    if (registrar_) {
        FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(
            registrar_, textureId);
    }
    (void)width; (void)height;
}

// ---------------- WebviewCefPlugin ----------------

void WebviewCefPlugin::RegisterWithRegistrar(flutter::PluginRegistrar* registrar) {
    // 不再 GetRegistrar 出 PluginRegistrarWindows,直接当作 PluginRegistrar 用。
    auto plugin = std::make_unique<WebviewCefPlugin>(registrar);
    plugin->EnsurePlugin();
    registrar->AddPlugin(std::move(plugin));
}

WebviewCefPlugin::WebviewCefPlugin(flutter::PluginRegistrar* registrar)
    : m_registrar(registrar) {
    m_plugin = std::make_unique<WebviewPlugin>();

    // 尝试拿 view + hwnd(失败也不致命)。
    auto* windows_registrar = dynamic_cast<flutter::PluginRegistrarWindows*>(registrar);
    if (windows_registrar) {
        auto view = windows_registrar->GetView();
        if (view) {
            m_hwnd = view->GetNativeWindow();
        }
    }

    m_channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        m_registrar->messenger(), "webview_cef",
        &flutter::StandardMethodCodec::GetInstance());
    m_channel->SetMethodCallHandler(
        [this](const auto& call, auto result) {
            this->HandleMethodCall(call, std::move(result));
        });
}

WebviewCefPlugin::~WebviewCefPlugin() {
    if (m_orig_wndproc && m_hwnd && IsWindow(m_hwnd)) {
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(m_orig_wndproc));
        m_orig_wndproc = nullptr;
    }
    m_renderers.clear();
    m_plugin.reset();
    m_channel.reset();
}

void WebviewCefPlugin::EnsurePlugin() {
    if (m_attached.exchange(true)) return;
    SetupBindings();
}

void WebviewCefPlugin::SendEvent(const std::string& name,
                                 flutter::EncodableValue payload) {
    if (!m_channel) return;
    auto args = std::make_unique<flutter::EncodableValue>(std::move(payload));
    m_channel->InvokeMethod(name, std::move(args));
}

void WebviewCefPlugin::SetupBindings() {
    auto* plugin = m_plugin.get();
    auto* renderer_holder = this;

    plugin->setInvokeMethodFunc([this](std::string method, WValue* body) {
        flutter::EncodableMap map;
        if (body) {
            // 简化:把所有键值打包为 map(实际生产环境建议展开为 EncodableValue 类型)。
            size_t n = webview_value_get_map_size(body);
            for (size_t i = 0; i < n; i++) {
                WValue* k = webview_value_get_map_key(body, i);
                WValue* v = webview_value_get_map_value(body, i);
                if (k && webview_value_get_type(k) == WValueType::String) {
                    std::string key = webview_value_get_string(k);
                    if (v) {
                        switch (webview_value_get_type(v)) {
                            case WValueType::Int:
                                map[flutter::EncodableValue(key)] =
                                    flutter::EncodableValue(
                                        webview_value_get_int(v));
                                break;
                            case WValueType::Bool:
                                map[flutter::EncodableValue(key)] =
                                    flutter::EncodableValue(
                                        webview_value_get_bool(v));
                                break;
                            case WValueType::Double:
                                map[flutter::EncodableValue(key)] =
                                    flutter::EncodableValue(
                                        webview_value_get_double(v));
                                break;
                            case WValueType::String:
                                map[flutter::EncodableValue(key)] =
                                    flutter::EncodableValue(
                                        std::string(webview_value_get_string(v)));
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }
        SendEvent(method, flutter::EncodableValue(std::move(map)));
    });

    plugin->setCreateTextureFunc(
        [renderer_holder]() -> std::shared_ptr<WebviewTexture> {
            auto* reg = reinterpret_cast<FlutterDesktopTextureRegistrarRef>(
                renderer_holder->m_registrar->texture_registrar());
            auto r = std::make_shared<WebviewTextureRenderer>(reg);
            return r;
        });
}

void WebviewCefPlugin::OnPaintCallback(int browserId, const void* buffer,
                                        int width, int height) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_renderers.find(browserId);
    if (it != m_renderers.end() && it->second) {
        it->second->onFrame(buffer, width, height);
    }
}

void WebviewCefPlugin::OnAcceleratedPaintCallback(int browserId,
                                                  const void* sharedHandle,
                                                  int width, int height, int format) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_renderers.find(browserId);
    if (it != m_renderers.end() && it->second) {
        it->second->onAcceleratedFrame(sharedHandle, width, height, format);
    }
}

// 把 EncodableValue 转回 WValue,用于把 Dart 调用转发给 plugin 的 HandleMethodCall。
namespace {

WValue* ToWValue(const flutter::EncodableValue& v);
WValue* ToWValueMap(const flutter::EncodableMap& m);
WValue* ToWValueList(const flutter::EncodableList& l);

WValue* ToWValue(const flutter::EncodableValue& v) {
    if (std::holds_alternative<int32_t>(v))
        return webview_value_new_int(std::get<int32_t>(v));
    if (std::holds_alternative<int64_t>(v))
        return webview_value_new_int(std::get<int64_t>(v));
    if (std::holds_alternative<bool>(v))
        return webview_value_new_bool(std::get<bool>(v));
    if (std::holds_alternative<double>(v))
        return webview_value_new_double(std::get<double>(v));
    if (std::holds_alternative<std::string>(v))
        return webview_value_new_string(std::get<std::string>(v).c_str());
    if (std::holds_alternative<flutter::EncodableMap>(v))
        return ToWValueMap(std::get<flutter::EncodableMap>(v));
    if (std::holds_alternative<flutter::EncodableList>(v))
        return ToWValueList(std::get<flutter::EncodableList>(v));
    return webview_value_new_null();
}

WValue* ToWValueMap(const flutter::EncodableMap& m) {
    WValue* out = webview_value_new_map();
    for (auto& kv : m) {
        WValue* key = ToWValue(kv.first);
        WValue* val = ToWValue(kv.second);
        webview_value_set(out, key, val);
        webview_value_unref(key);
        webview_value_unref(val);
    }
    return out;
}

WValue* ToWValueList(const flutter::EncodableList& l) {
    WValue* out = webview_value_new_list();
    for (auto& v : l) {
        WValue* item = ToWValue(v);
        webview_value_append(out, item);
        webview_value_unref(item);
    }
    return out;
}

}  // namespace

void WebviewCefPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

    auto* plugin = m_plugin.get();
    WValue* args = call.arguments() ? ToWValue(*call.arguments()) : nullptr;

    auto reply = [result = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result))](int code, WValue* body) mutable {
        if (code == 0) {
            result->NotImplemented();
            return;
        }
        if (!body) {
            result->Success();
            return;
        }
        // 把 WValue 转回 EncodableValue。
        flutter::EncodableValue v = flutter::EncodableValue();
        // 简化:只处理 list<int>
        if (webview_value_get_type(body) == WValueType::List) {
            flutter::EncodableList list;
            for (size_t i = 0; i < webview_value_get_list_size(body); i++) {
                WValue* item = webview_value_get_list_item(body, i);
                if (item && webview_value_get_type(item) == WValueType::Int) {
                    list.push_back(flutter::EncodableValue(
                        static_cast<int32_t>(webview_value_get_int(item))));
                }
            }
            result->Success(flutter::EncodableValue(std::move(list)));
        } else {
            result->Success();
        }
    };

    plugin->HandleMethodCall(call.method_name(), args, reply);

    if (args) webview_value_unref(args);
}

LRESULT CALLBACK WebviewCefPlugin::handleMessageProc(HWND hwnd, UINT message,
                                                     WPARAM wParam, LPARAM lParam) {
    // 默认:把按键消息转给 CEF(细节由 runner 调用 CefBrowserHost::SendKeyEvent)。
    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

void WebviewCefPlugin::HandleImeMessage(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)message; (void)wp; (void)lp;
}

}  // namespace webview_cef
