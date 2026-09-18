// webview_plugin.cc — WebviewPlugin + CEF 进程入口。

// 必须先于 windows.h 包含,防止 min/max 宏与 CEF 模板代码冲突。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_plugin.h"

// 二次清理:某些 CEF 头在 include 链中又会间接拉入 windows.h。
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_task.h"

namespace webview_cef {

namespace {

CefMainArgs g_main_args;
CefRefPtr<WebviewApp> g_app;
bool isCefInitialized = false;
bool isCefShutdown = false;
std::string userAgent;

}  // namespace

WebviewPlugin::WebviewPlugin() {
    m_handler = new WebviewHandler();
    m_app = nullptr;  // 由 startCEF 创建并使用
}

WebviewPlugin::~WebviewPlugin() {
    m_renderers.clear();
    m_handler = nullptr;
}

void WebviewPlugin::initCallback() {
    m_init = true;
}

void WebviewPlugin::uninitCallback() {
    m_init = false;
}

void WebviewPlugin::setInvokeMethodFunc(
    std::function<void(std::string, WValue*)> func) {
    m_invokeFunc = std::move(func);
}

void WebviewPlugin::setCreateTextureFunc(
    std::function<std::shared_ptr<WebviewTexture>()> func) {
    m_createTextureFunc = std::move(func);
}

bool WebviewPlugin::getAnyBrowserFocused() {
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        auto it = m_renderers.find(id);
        if (it != m_renderers.end() && it->second && it->second->isFocused) {
            return true;
        }
    }
    return false;
}

bool WebviewPlugin::isEditableFocused() {
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        auto it = m_renderers.find(id);
        if (it != m_renderers.end() && it->second && it->second->editableFocused) {
            return true;
        }
    }
    return false;
}

bool WebviewPlugin::isComposing() {
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        auto it = m_renderers.find(id);
        if (it != m_renderers.end() && it->second && it->second->composing) {
            return true;
        }
    }
    return false;
}

void WebviewPlugin::tickBeginFrame() {
    // 单线程模式下由 doMessageLoopWork 驱动,多线程模式下不需要。
}

int WebviewPlugin::focusedBrowserId() {
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        auto it = m_renderers.find(id);
        if (it != m_renderers.end() && it->second && it->second->isFocused) {
            return id;
        }
    }
    return -1;
}

void WebviewPlugin::setComposingForBrowser(int browserId, bool composing) {
    auto it = m_renderers.find(browserId);
    if (it != m_renderers.end() && it->second) {
        it->second->composing = composing;
    }
}

int WebviewPlugin::cursorAction(WValue* args, const std::string& name) {
    if (!args) return 1;
    WValue* v = webview_value_get_map_value(args, name.c_str());
    if (!v) return 1;
    int x = static_cast<int>(webview_value_get_int(
        webview_value_get_map_value(v, "x")));
    int y = static_cast<int>(webview_value_get_int(
        webview_value_get_map_value(v, "y")));
    int bid = focusedBrowserId();
    if (bid < 0) return 1;
    auto it = m_renderers.find(bid);
    if (it == m_renderers.end() || !it->second) return 1;
    (void)x; (void)y;
    return 1;
}

void WebviewPlugin::imeSetCompositionNative(const std::wstring& text, int cursor) {
    int bid = focusedBrowserId();
    if (bid < 0) return;
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        if (id == bid) {
            setComposingForBrowser(id, true);
        }
    }
    (void)text; (void)cursor;
}

void WebviewPlugin::imeCommitTextNative(const std::wstring& text) {
    int bid = focusedBrowserId();
    if (bid < 0) return;
    auto it = m_renderers.find(bid);
    if (it != m_renderers.end() && it->second) {
        it->second->composing = false;
    }
    (void)text;
}

void WebviewPlugin::imeFinishCompositionNative() {
    int bid = focusedBrowserId();
    if (bid < 0) return;
    setComposingForBrowser(bid, false);
}

void WebviewPlugin::sendKeyEvent(CefKeyEvent& ev) {
    int bid = focusedBrowserId();
    if (bid < 0) return;
    auto ids = m_handler->getActiveBrowserIds();
    for (auto id : ids) {
        if (id == bid) {
            auto it = m_renderers.find(id);
            if (it != m_renderers.end() && it->second && it->second->browser) {
                it->second->browser->GetHost()->SendKeyEvent(ev);
                return;
            }
        }
    }
    (void)ev;
}

void WebviewPlugin::HandleMethodCall(
    std::string name, WValue* values,
    std::function<void(int, WValue*)> result) {

    auto map_event = [&](const std::string& method, WValue* body) {
        if (m_invokeFunc) m_invokeFunc(method, body);
    };

    if (name == "init") {
        if (!isCefInitialized) {
            if (values) {
                WValue* ua = webview_value_get_map_value(values, "userAgent");
                if (ua) userAgent = webview_value_get_string(ua);
            }
            startCEF();
        }
        initCallback();
        result(1, nullptr);
    } else if (name == "quit") {
        stopCEF();
        result(1, nullptr);
    } else if (name == "create") {
        std::string url = webview_value_get_string(values);
        m_handler->createBrowser(url, [=, this](int browserId) {
            std::shared_ptr<WebviewTexture> renderer = m_createTextureFunc();
            m_renderers[browserId] = renderer;
            WValue* response = webview_value_new_list();
            webview_value_append(response, webview_value_new_int(browserId));
            webview_value_append(response, webview_value_new_int(renderer->textureId));
            result(1, response);
            webview_value_unref(response);
        });
    } else if (name == "close") {
        int browserId = static_cast<int>(webview_value_get_int(values));
        m_handler->closeBrowser(browserId);
        auto it = m_renderers.find(browserId);
        if (it != m_renderers.end() && it->second) {
            m_renderers.erase(it);
        }
        result(1, nullptr);
    } else if (name == "loadUrl") {
        int browserId = static_cast<int>(webview_value_get_int(values));
        std::string url = webview_value_get_string(
            webview_value_get_map_value(values, "url"));
        auto ids = m_handler->getActiveBrowserIds();
        for (auto id : ids) {
            if (id == browserId) {
                auto it = m_renderers.find(id);
                if (it != m_renderers.end() && it->second && it->second->browser) {
                    it->second->browser->GetMainFrame()->LoadURL(CefString(url));
                }
            }
        }
        result(1, nullptr);
    } else if (name == "goBack" || name == "goForward" ||
               name == "reload" || name == "stopLoad" ||
               name == "openDevTools") {
        int browserId = static_cast<int>(webview_value_get_int(values));
        auto ids = m_handler->getActiveBrowserIds();
        for (auto id : ids) {
            if (id == browserId) {
                auto it = m_renderers.find(id);
                if (it != m_renderers.end() && it->second && it->second->browser) {
                    auto host = it->second->browser->GetHost();
                    auto browser = it->second->browser;
                    if (name == "goBack") browser->GoBack();
                    else if (name == "goForward") browser->GoForward();
                    else if (name == "reload") browser->Reload();
                    else if (name == "stopLoad") browser->StopLoad();
                    else if (name == "openDevTools") host->ShowDevTools(
                        CefWindowInfo(), nullptr,
                        CefBrowserSettings(), CefPoint());
                }
            }
        }
        result(1, nullptr);
    } else if (name == "executeJavaScript") {
        int browserId = static_cast<int>(webview_value_get_int(values));
        std::string code = webview_value_get_string(
            webview_value_get_map_value(values, "code"));
        auto ids = m_handler->getActiveBrowserIds();
        for (auto id : ids) {
            if (id == browserId) {
                auto it = m_renderers.find(id);
                if (it != m_renderers.end() && it->second && it->second->browser) {
                    it->second->browser->GetMainFrame()->ExecuteJavaScript(
                        CefString(code), it->second->browser->GetMainFrame()->GetURL(), 0);
                }
            }
        }
        result(1, nullptr);
    } else if (name == "setCookie" || name == "deleteCookie" ||
               name == "visitAllCookies") {
        // 简化实现:返回空列表。
        WValue* response = webview_value_new_map();
        result(1, response);
        webview_value_unref(response);
    } else if (name == "hasNativeKeySupport") {
        result(1, webview_value_new_bool(true));
    } else {
        result(0, nullptr);
    }
    (void)map_event;
}

int initCEFProcesses(CefMainArgs args) {
    g_main_args = args;
    return initCEFProcesses();
}

int initCEFProcesses() {
    g_app = new WebviewApp();
    int rc = CefExecuteProcess(g_main_args, g_app, nullptr);
    if (rc < 0) {
        // Broker / browser 主进程:把 CEF 完全初始化好,这样 fork 子进程时
        // ICU / request context 都已经在 broker 上准备好了。
        CefSettings cefs;
        cefs.windowless_rendering_enabled = true;
        cefs.no_sandbox = true;
        cefs.multi_threaded_message_loop = true;
        // log_severity = DISABLE: 阻止 CEF 子进程往 stdout/stderr 写 log
        // (Windows GUI subsystem 下,这种写会触发 conhost 黑窗口)。
        cefs.log_severity = LOGSEVERITY_DISABLE;
        if (!userAgent.empty()) {
            CefString(&cefs.user_agent_product) = userAgent;
        }
        CefInitialize(g_main_args, cefs, g_app, nullptr);
        isCefInitialized = true;
    }
    return rc;
}

void startCEF() {
    // 现在 initCEFProcesses 已经做了 CefInitialize,这里只做幂等保护。
    if (isCefInitialized) return;
    initCEFProcesses();
}

void doMessageLoopWork() {
    if (isCefInitialized && !isCefShutdown) {
        CefDoMessageLoopWork();
    }
}

void SwapBufferFromBgraToRgba(void* _dest, const void* _src,
                              int width, int height) {
    int32_t* dest = static_cast<int32_t*>(_dest);
    const int32_t* src = static_cast<const int32_t*>(_src);
    int32_t rgba, bgra;
    int length = width * height;
    for (int i = 0; i < length; i++) {
        bgra = src[i];
        rgba = (bgra & 0x00ff0000) >> 16
             | (bgra & 0xff00ff00)
             | (bgra & 0x000000ff) << 16;
        dest[i] = rgba;
    }
}

void stopCEF() {
    if (isCefShutdown) return;
    isCefShutdown = true;
    CefShutdown();
}

}  // namespace webview_cef
