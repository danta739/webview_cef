// webview_app_views.h — CEF Views 框架相关扩展(预留,目前未启用)。

#ifndef WEBVIEW_APP_VIEWS_H
#define WEBVIEW_APP_VIEWS_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cef_app.h"

namespace webview_cef {

// 简易开关:目前 Views 框架不在主流程中使用,但保留入口供未来扩展。
class WebviewAppViews {
 public:
    static void OnContextInitialized() {}
};

}  // namespace webview_cef

#endif  // WEBVIEW_APP_VIEWS_H
