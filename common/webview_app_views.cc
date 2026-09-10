// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "webview_app_views.h"

#include "include/wrapper/cef_helpers.h"

namespace webview_app_views {

SimpleWindowDelegate::SimpleWindowDelegate(
    CefRefPtr<CefBrowserView> browser_view)
    : browser_view_(browser_view) {}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
  // 添加浏览器视图并显示窗口。
  window->AddChildView(browser_view_);
  window->Show();

  // 将键盘焦点给予浏览器视图。
  browser_view_->RequestFocus();
}

void SimpleWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  browser_view_ = nullptr;
}

bool SimpleWindowDelegate::CanClose(CefRefPtr<CefWindow> window) {
  // 如果浏览器允许则允许窗口关闭。
  CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
  if (browser)
    return browser->GetHost()->TryCloseBrowser();
  return true;
}

CefSize SimpleWindowDelegate::GetPreferredSize(CefRefPtr<CefView> view) {
  return CefSize(1280, 720);
}

SimpleBrowserViewDelegate::SimpleBrowserViewDelegate() {}

bool SimpleBrowserViewDelegate::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  // 为弹出窗口创建一个新的顶层 Window。创建后它将自动显示。
  CefWindow::CreateTopLevelWindow(
      new SimpleWindowDelegate(popup_browser_view));

  // 我们创建了该 Window。返回 false 表示已自行处理弹窗。
  return false;
}

}  // namespace webview_app_views
