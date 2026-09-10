// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef WEBVIEW_APP_VIEWS_H_
#define WEBVIEW_APP_VIEWS_H_

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"

namespace webview_app_views {

// 使用 Views 框架时，此对象为承载基于 Views 的浏览器的 CefWindow 提供委托实现。
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view);

  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  SimpleBrowserViewDelegate();

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override;

 private:
  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

}  // namespace webview_app_views

#endif  // WEBVIEW_APP_VIEWS_H_
