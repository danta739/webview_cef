// webview_cookieVisitor.cc

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_cookieVisitor.h"

namespace webview_cef {

bool WebviewCookieVisitor::Visit(const CefCookie& cookie,
                                 int count,
                                 int total,
                                 bool& deleteCookie) {
    if (!cb_) return true;
    Entry e;
    e.name = CefString(const_cast<cef_string_t*>(&cookie.name)).ToString();
    e.value = CefString(const_cast<cef_string_t*>(&cookie.value)).ToString();
    e.domain = CefString(const_cast<cef_string_t*>(&cookie.domain)).ToString();
    e.path = CefString(const_cast<cef_string_t*>(&cookie.path)).ToString();
    e.secure = cookie.secure != 0;
    e.httponly = cookie.httponly != 0;
    e.has_expires = cookie.has_expires != 0;
    bool has_more = count < total - 1;
    cb_(has_more, e);
    return true;  // 继续
}

}  // namespace webview_cef
