// webview_windowed.cc — 屏上渲染子窗口同步实现。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_windowed.h"

#include <windows.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <atomic>
#include <mutex>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"

namespace webview_windowed {

WindowedClient::WindowedClient(WindowedCallbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

void WindowedClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    if (callbacks_.on_browser_closed && browser) {
        callbacks_.on_browser_closed(browser->GetIdentifier());
    }
}

void WindowedClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 TransitionType transition_type) {
    CEF_REQUIRE_UI_THREAD();
    if (!browser || !frame || !frame->IsMain()) return;
    if (callbacks_.on_load_start) {
        callbacks_.on_load_start(browser->GetIdentifier(),
                                 frame->GetURL().ToString());
    }
}

void WindowedClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();
    if (!browser || !frame || !frame->IsMain()) return;
    if (callbacks_.on_load_end) {
        callbacks_.on_load_end(browser->GetIdentifier(),
                               frame->GetURL().ToString());
    }
}

void WindowedClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    if (callbacks_.on_title_changed && browser) {
        callbacks_.on_title_changed(browser->GetIdentifier(),
                                    title.ToString());
    }
}

void WindowedClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     const CefString& url) {
    CEF_REQUIRE_UI_THREAD();
    if (!browser || !frame || !frame->IsMain()) return;
    if (callbacks_.on_url_changed) {
        callbacks_.on_url_changed(browser->GetIdentifier(), url.ToString());
    }
}

bool WindowedClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                      cef_log_severity_t level,
                                      const CefString& message,
                                      const CefString& source,
                                      int line) {
    if (callbacks_.on_console_message && browser) {
        callbacks_.on_console_message(browser->GetIdentifier(),
                                      static_cast<int>(level),
                                      message.ToString(),
                                      source.ToString(),
                                      line);
    }
    return false;
}

namespace {

struct ParentBinding {
    HWND hwnd = nullptr;
    WNDPROC orig_wndproc = nullptr;
    std::atomic<bool> active{false};
};

ParentBinding g_binding;
std::mutex g_mutex;

std::unordered_map<int, WindowedChild> g_children;

void PostSyncChildRectTask(int browser_id) {
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::BindOnce(
            [](int bid) { SyncChildRect(bid); }, browser_id));
    } else {
        SyncChildRect(browser_id);
    }
}

bool GetParentClientRect(HWND parent, RECT& out) {
    if (!parent || !::IsWindow(parent)) return false;
    return ::GetClientRect(parent, &out) != FALSE;
}

LRESULT CALLBACK ParentWndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HWND binding_hwnd = nullptr;
    WNDPROC binding_orig = nullptr;
    bool binding_active = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        binding_hwnd = g_binding.hwnd;
        binding_orig = g_binding.orig_wndproc;
        binding_active = g_binding.active.load();
    }

    if (!binding_active || binding_hwnd != hwnd) {
        return ::DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
        case WM_MOVE:
        case WM_DPICHANGED: {
            for (auto& kv : g_children) {
                PostSyncChildRectTask(kv.first);
            }
            break;
        }
        case WM_DESTROY: {
            CefPostTask(TID_UI, base::BindOnce(
                []() { CloseAllWindowedBrowsers(true); }));
            break;
        }
        default:
            break;
    }

    return ::CallWindowProc(binding_orig, hwnd, msg, wp, lp);
}

}  // namespace

void BindParentWindow(HWND parent_hwnd) {
    if (!parent_hwnd || !::IsWindow(parent_hwnd)) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_binding.active && g_binding.hwnd == parent_hwnd) return;

    if (g_binding.active) {
        if (g_binding.orig_wndproc) {
            ::SetWindowLongPtr(g_binding.hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_binding.orig_wndproc));
        }
    }

    g_binding.hwnd = parent_hwnd;
    g_binding.orig_wndproc = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtr(parent_hwnd, GWLP_WNDPROC,
                           reinterpret_cast<LONG_PTR>(ParentWndProcHook)));
    g_binding.active = (g_binding.orig_wndproc != nullptr);
}

void UnbindParentWindow() {
    std::lock_guard<std::mutex> lock(g_mutex);
    bool was_active = g_binding.active.load();
    HWND hwnd = g_binding.hwnd;
    WNDPROC orig = g_binding.orig_wndproc;

    if (!was_active) return;

    if (orig && ::IsWindow(hwnd)) {
        ::SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                           reinterpret_cast<LONG_PTR>(orig));
    }
    g_binding.hwnd = nullptr;
    g_binding.orig_wndproc = nullptr;
    g_binding.active = false;
}

CefRefPtr<CefBrowser> CreateWindowedBrowser(
    HWND parent_hwnd,
    const CefString& url,
    const WindowedPlacement& placement,
    const WindowedCallbacks& callbacks) {

    CEF_REQUIRE_UI_THREAD();
    if (!parent_hwnd || !::IsWindow(parent_hwnd)) return nullptr;

    CefWindowInfo window_info;
    window_info.SetAsChild(parent_hwnd,
                           CefRect(placement.x, placement.y,
                                   placement.width, placement.height));

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 0;

    CefRefPtr<WindowedClient> client = new WindowedClient(callbacks);

    CefRefPtr<CefBrowser> browser =
        CefBrowserHost::CreateBrowserSync(window_info, client,
                                          url, browser_settings,
                                          /*extra_info=*/nullptr,
                                          /*request_context=*/nullptr);
    if (!browser) return nullptr;

    HWND child_hwnd = browser->GetHost()->GetWindowHandle();
    client->SetBrowser(browser);
    WindowedChild entry;
    entry.browser = browser;
    entry.hwnd = child_hwnd;
    entry.client = client;
    g_children[browser->GetIdentifier()] = entry;
    return browser;
}

void SyncChildRect(int browser_id) {
    CEF_REQUIRE_UI_THREAD();

    auto it = g_children.find(browser_id);
    if (it == g_children.end()) return;

    HWND parent;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        parent = g_binding.hwnd;
    }
    if (!parent || !::IsWindow(parent)) return;

    RECT rc;
    if (!GetParentClientRect(parent, rc)) return;

    HWND child = it->second.hwnd;
    if (!child || !::IsWindow(child)) return;

    int diff_w = rc.right - rc.left;
    int diff_h = rc.bottom - rc.top;
    int w = diff_w > 1 ? diff_w : 1;
    int h = diff_h > 1 ? diff_h : 1;
    ::MoveWindow(child, 0, 0, w, h, TRUE);
}

void SyncChildRectTo(int browser_id, int x, int y, int w, int h) {
    CEF_REQUIRE_UI_THREAD();

    auto it = g_children.find(browser_id);
    if (it == g_children.end() || !it->second.hwnd) return;

    int clamped_w = w > 1 ? w : 1;
    int clamped_h = h > 1 ? h : 1;
    ::MoveWindow(it->second.hwnd, x, y, clamped_w, clamped_h, TRUE);
}

void CloseWindowedBrowser(int browser_id) {
    CEF_REQUIRE_UI_THREAD();
    auto it = g_children.find(browser_id);
    if (it == g_children.end()) return;
    if (it->second.browser) {
        it->second.browser->GetHost()->CloseBrowser(/*force_close=*/true);
    }
}

void CloseAllWindowedBrowsers(bool force) {
    CEF_REQUIRE_UI_THREAD();
    auto copy = g_children;
    for (auto& kv : copy) {
        if (kv.second.browser) {
            kv.second.browser->GetHost()->CloseBrowser(force);
        }
    }
    g_children.clear();
}

bool IsWindowedModeBound() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_binding.active && ::IsWindow(g_binding.hwnd);
}

void NotifyParentResized() {
    CefPostTask(TID_UI, base::BindOnce([]() {
        HWND parent;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            parent = g_binding.hwnd;
        }
        if (!parent || !::IsWindow(parent)) return;
        for (auto& kv : g_children) {
            SyncChildRect(kv.first);
        }
    }));
}

CefRefPtr<WindowedClient> GetWindowedClient(int browser_id) {
    auto it = g_children.find(browser_id);
    if (it == g_children.end()) return nullptr;
    return it->second.client;
}

}  // namespace webview_windowed
