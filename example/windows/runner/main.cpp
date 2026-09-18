#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <windows.h>
#include <cstdio>
#include <string>

#include "flutter_window.h"
#include "utils.h"
#include "webview_cef/webview_cef_plugin_c_api.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev,
                      _In_ wchar_t* command_line, _In_ int show_command) {
  // 早期日志:通过 OutputDebugStringA 输出,不创建临时文件。
  auto earlyLog = [](const std::string& msg) {
    ::OutputDebugStringA(msg.c_str());
  };
  earlyLog("wWinMain: enter");

  // 先让 plugin DLL 内的 CEF 子进程分派逻辑跑一遍:
  // 如果当前进程是 CEF 子进程(渲染进程/GPU进程等),它会立即返回非负的退出码,
  // 我们直接退出,不再初始化 Flutter。
  const int helper_exit_code = initCEFProcesses(instance);
  earlyLog("wWinMain: initCEFProcesses returned " + std::to_string(helper_exit_code));
  if (helper_exit_code >= 0) {
    return helper_exit_code;
  }

  // Attach to console when present (e.g., 'flutter run') or create a
  // new console when running with a debugger.
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && ::IsDebuggerPresent()) {
    CreateAndAttachConsole();
  }

  // Initialize COM, so that it is available for use in the library and/or
  // plugins.
  ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  flutter::DartProject project(L"data");

  std::vector<std::string> command_line_arguments =
      GetCommandLineArguments();

  project.set_dart_entrypoint_arguments(std::move(command_line_arguments));

  FlutterWindow window(project);
  Win32Window::Point origin(10, 10);
  Win32Window::Size size(1280, 720);
  if (!window.Create(L"webview_cef_example", origin, size)) {
    return EXIT_FAILURE;
  }
  window.SetQuitOnClose(true);

  ::MSG msg;
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  ::CoUninitialize();
  return EXIT_SUCCESS;
}