// webview_cookieVisitor.h — CEF Cookie 访问器。

#ifndef WEBVIEW_COOKIE_VISITOR_H
#define WEBVIEW_COOKIE_VISITOR_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <functional>

#include "include/cef_cookie.h"

namespace webview_cef {

class WebviewCookieVisitor : public CefCookieVisitor {
 public:
    struct Entry {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        bool secure = false;
        bool httponly = false;
        bool has_expires = false;
    };

    using Callback = std::function<void(bool has_more, const Entry& e)>;

    explicit WebviewCookieVisitor(Callback cb) : cb_(std::move(cb)) {}
    ~WebviewCookieVisitor() override = default;

    bool Visit(const CefCookie& cookie,
               int count,
               int total,
               bool& deleteCookie) override;

 private:
    Callback cb_;
    IMPLEMENT_REFCOUNTING(WebviewCookieVisitor);
};

}  // namespace webview_cef

#endif  // WEBVIEW_COOKIE_VISITOR_H
