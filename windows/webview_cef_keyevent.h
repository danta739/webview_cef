// webview_cef_keyevent.h — Windows key event 与 CefKeyEvent 互转。

#ifndef WEBVIEW_CEF_KEYEVENT_H_
#define WEBVIEW_CEF_KEYEVENT_H_

#include "cef_browser.h"

namespace webview_cef {

// 把 Windows WM_KEYDOWN / WM_KEYUP / WM_CHAR 转换为 CefKeyEvent。
inline CefKeyEvent ToCefKeyEvent(WPARAM wParam, LPARAM lParam,
                                 int message_type) {
    CefKeyEvent ev;
    if (message_type == WM_CHAR) {
        ev.type = KEYEVENT_CHAR;
    } else if (message_type == WM_KEYDOWN) {
        ev.type = KEYEVENT_RAWKEYDOWN;
    } else if (message_type == WM_KEYUP) {
        ev.type = KEYEVENT_KEYUP;
    } else {
        ev.type = KEYEVENT_RAWKEYDOWN;
    }
    ev.windows_key_code = static_cast<int>(wParam);
    ev.native_key_code = lParam & 0xFFFF;
    ev.modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)   ev.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_CONTROL) & 0x8000) ev.modifiers |= EVENTFLAG_CONTROL_DOWN;
    if (GetKeyState(VK_MENU) & 0x8000)    ev.modifiers |= EVENTFLAG_ALT_DOWN;
    if (GetKeyState(VK_CAPITAL) & 0x0001) ev.modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    ev.is_system_key = (lParam & (1 << 29)) != 0;
    return ev;
}

}  // namespace webview_cef

#endif  // WEBVIEW_CEF_KEYEVENT_H_
