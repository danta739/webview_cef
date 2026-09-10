#pragma once
#include "include/cef_base.h"

bool IsKeyDown(WPARAM wparam)
{
	return (GetKeyState((int)wparam) & 0x8000) != 0;
}

int GetCefKeyboardModifiers(WPARAM wparam, LPARAM lparam)
{
	int modifiers = 0;
	if (IsKeyDown(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (IsKeyDown(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (IsKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// GetKeyState 返回的最低位表示"已切换"。
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam)
	{
	case VK_RETURN:
		if ((lparam >> 16) & KF_EXTENDED)
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		if (!((lparam >> 16) & KF_EXTENDED))
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_NUMLOCK:
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	case VK_DIVIDE:
	case VK_MULTIPLY:
	case VK_SUBTRACT:
	case VK_ADD:
	case VK_DECIMAL:
	case VK_CLEAR:
		modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_SHIFT:
		if (IsKeyDown(VK_LSHIFT))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RSHIFT))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_CONTROL:
		if (IsKeyDown(VK_LCONTROL))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RCONTROL))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_MENU:
		if (IsKeyDown(VK_LMENU))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RMENU))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_LWIN:
		modifiers |= EVENTFLAG_IS_LEFT;
		break;
	case VK_RWIN:
		modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	}
	return modifiers;
}

CefKeyEvent getCefKeyEvent(UINT message, WPARAM wparam, LPARAM lparam)
{
    CefKeyEvent event;
    event.windows_key_code = (int)wparam;
    event.native_key_code = (int)lparam;
    event.is_system_key = message == WM_SYSCHAR || message == WM_SYSKEYDOWN ||
                          message == WM_SYSKEYUP;

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        event.type = KEYEVENT_RAWKEYDOWN;
    else if (message == WM_KEYUP || message == WM_SYSKEYUP)
        event.type = KEYEVENT_KEYUP;
    else
        event.type = KEYEVENT_CHAR;
    event.modifiers = GetCefKeyboardModifiers(wparam, lparam);

    // 模拟来自 src/ui/events/win/events_win_utils.cc: GetModifiersFromKeyState 的 alt-gr 检查行为
    if ((event.type == KEYEVENT_CHAR) && IsKeyDown(VK_RMENU))
    {
        // 从 PlatformKeyMap::UsesAltGraph 反向检测 AltGr
        // 不检查 ctrl-alt 的所有组合，仅检查当前字符
        HKL current_layout = ::GetKeyboardLayout(0);

        // https://docs.microsoft.com/en-gb/windows/win32/api/winuser/nf-winuser-vkkeyscanexw
        // ... 高位字节包含 shift 状态，
        // 可以是以下标志位的组合。
        // 1 按下了任一 SHIFT 键。
        // 2 按下了任一 CTRL 键。
        // 4 按下了任一 ALT 键。
        SHORT scan_res = ::VkKeyScanExW((WCHAR)wparam, current_layout);
        constexpr auto ctrlAlt = (2 | 4);
        if (((scan_res >> 8) & ctrlAlt) == ctrlAlt)
        { // 按下了 ctrl-alt
            event.modifiers &= ~(EVENTFLAG_CONTROL_DOWN | EVENTFLAG_ALT_DOWN);
            event.modifiers |= EVENTFLAG_ALTGR_DOWN;
        }
    }
    return event;
}