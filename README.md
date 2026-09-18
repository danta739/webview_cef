# WebView CEF

Flutter Windows WebView plugin based on [CEF](https://bitbucket.org/chromiumembedded/cef) (Chromium Embedded Framework). The plugin uses CEF's off-screen rendering mode and presents the browser surface through a Flutter `Texture`.

> This repository currently supports **Windows x64 only**.

## Features

- Chromium 149 engine through CEF 149.
- Off-screen rendering with optional D3D11 GPU shared-texture rendering.
- CJK/IME composition through the native Windows IME pipeline.
- Multiple independent WebView instances.
- Navigation, title/URL/load events, mouse input and DevTools.
- JavaScript evaluation and JavaScript-to-Dart channels.
- Cookie management and document user-script injection.
- **Windowed (on-screen) rendering mode** — overlay a real CEF child window on the Flutter window. See [WINDOWED_RENDERING.md](WINDOWED_RENDERING.md) for details.

## Requirements

- Windows 10 or newer, x64.
- Flutter >= 3.27.0 and Dart >= 3.6.0.
- Visual Studio with the Desktop development with C++ workload.
- CMake 3.14 or newer, supplied by the Flutter/Visual Studio toolchain.
- A C++20-compatible MSVC toolchain.

## Add The Plugin

From an existing Flutter Windows project:

```powershell
flutter pub add webview_cef
flutter config --enable-windows-desktop
flutter pub get
```

The package registers the Windows plugin through `WebviewCefPluginCApi`. No macOS, Linux or eLinux setup is required.

## Windows Runner Integration

CEF uses separate renderer, GPU and utility processes. The plugin must initialize CEF before Flutter creates its main window. Include the generated C API header and call `initCEFProcesses` as the first operation in `wWinMain`:

```cpp
#include "webview_cef/webview_cef_plugin_c_api.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev,
                      _In_ wchar_t* command_line, _In_ int show_command) {
  const int exit_code = initCEFProcesses(instance);
  if (exit_code >= 0) {
    return exit_code;
  }

  // Existing Flutter runner initialization follows.
}
```

Forward Windows messages to CEF after `DispatchMessage` in the runner message loop. This is required for keyboard input and for native-to-Flutter method-channel callbacks:

```cpp
MSG message;
while (GetMessage(&message, nullptr, 0, 0)) {
  TranslateMessage(&message);
  DispatchMessage(&message);
  handleWndProcForCEF(message.hwnd, message.message,
                      message.wParam, message.lParam);
}
```

The plugin subclasses the Flutter window for `WM_IME_*` messages. No additional runner code is needed for Chinese, Japanese or Korean IME composition.

## CEF Download And Build

On the first Windows build, `windows/CMakeLists.txt` uses `third/download.cmake` to download the pinned official CEF Standard Distribution into `third/cef`. The CEF `libcef_dll_wrapper` is then compiled from source. The download is roughly 330 MB and the first build is significantly slower.

```powershell
flutter pub get
flutter build windows --debug
```

The CEF version is defined by `CEF_VERSION` in `third/download.cmake`. To update CEF, change that value and rebuild. Do not manually copy CEF binaries into the Flutter runner.

## Runtime Files

The Windows CMake target bundles the CEF runtime beside the Flutter executable, including:

- `libcef.dll`
- CEF resource `.pak` files
- `locales/`
- ANGLE and SwiftShader runtime libraries when provided by the CEF distribution

When packaging the application, package the complete generated bundle directory. Copying only the Flutter executable will result in a blank WebView or a CEF startup failure.

## Flutter Usage

Initialize the manager once, create a controller, then initialize the browser URL:

```dart
import 'package:flutter/material.dart';
import 'package:webview_cef/webview_cef.dart';

class CefView extends StatefulWidget {
  const CefView({super.key});

  @override
  State<CefView> createState() => _CefViewState();
}

class _CefViewState extends State<CefView> {
  late final WebViewController controller;

  @override
  void initState() {
    super.initState();
    controller = WebviewManager().createWebView(
      loading: const Center(child: CircularProgressIndicator()),
    );
    _initialize();
  }

  Future<void> _initialize() async {
    await WebviewManager().initialize(userAgent: 'my-app/1.0');
    controller.setWebviewListener(WebviewEventsListener(
      onTitleChanged: (title) => debugPrint(title),
      onUrlChanged: (url) => debugPrint(url),
      onLoadEnd: (controller, url) => debugPrint('loaded: $url'),
    ));
    await controller.initialize('https://example.com');
  }

  @override
  void dispose() {
    controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<bool>(
      valueListenable: controller,
      builder: (context, ready, child) {
        return ready ? controller.webviewWidget : controller.loadingWidget;
      },
    );
  }
}
```

Call `WebviewManager().quit()` once when the whole application is shutting down, after all controllers have been disposed.

## Common APIs

```dart
controller.loadUrl('https://example.com');
controller.reload();
controller.goBack();
controller.goForward();
controller.openDevTools();

controller.executeJavaScript("document.title = 'CEF'");
final value = await controller.evaluateJavascript('1 + 1');

await WebviewManager().setCookie('example.com', 'session', 'value');
await WebviewManager().deleteCookie('example.com', 'session');
final cookies = await WebviewManager().visitAllCookies();
```

JavaScript can call Dart through a named channel:

```dart
controller.setJavaScriptChannels({
  JavascriptChannel(
    name: 'Print',
    onMessageReceived: (message) {
      debugPrint(message.message);
      controller.sendJavaScriptChannelCallBack(
        false,
        '{"ok":true}',
        message.callbackId,
        message.frameId,
      );
    },
  ),
});
```

## Windows Build Options

The following CMake options are available in `windows/CMakeLists.txt`:

| Option | Default | Description |
| --- | --- | --- |
| `WEBVIEW_CEF_GPU_TEXTURE` | `ON` | Uses CEF D3D11 shared textures and Flutter GPU textures. Set to `OFF` for the CPU pixel-buffer path. |
| `WEBVIEW_CEF_USE_DEBUG_CEF` | `OFF` | Links CEF Debug binaries. Keep `OFF` unless debugging CEF itself; CEF Debug DCHECKs can interfere with off-screen IME rendering. |

## Example Project

The repository contains a complete sample in [`example/`](example/). Run it with:

```powershell
cd example
flutter pub get
flutter run -d windows
```

## Troubleshooting

- **CEF download repeats**: remove an incomplete `third/cef` directory and rebuild with network access.
- **Blank WebView after packaging**: verify that `libcef.dll`, `.pak` files, `locales/`, and the graphics runtime files are beside the executable.
- **Keyboard input does not work**: verify that `handleWndProcForCEF` is called after `DispatchMessage`.
- **The child process starts as the main Flutter process**: verify that `initCEFProcesses(instance)` is the first operation in `wWinMain` and that non-negative return values are returned immediately.
- **GPU rendering is unavailable**: build with `WEBVIEW_CEF_GPU_TEXTURE=OFF` to validate the CPU fallback path.

## License

[Apache License 2.0](LICENSE)
