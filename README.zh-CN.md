# WebView CEF

基于 [CEF](https://bitbucket.org/chromiumembedded/cef)（Chromium Embedded Framework）的 Flutter Windows WebView 插件。插件使用 CEF 离屏渲染，并通过 Flutter `Texture` 将浏览器画面显示在 Flutter 界面中。

> 当前仓库只支持 **Windows x64**。

## 功能

- 基于 CEF 149（Chromium 149）。
- 离屏渲染，支持 D3D11 GPU 共享纹理，也支持 CPU 像素缓冲回退。
- 通过 Windows 原生 IME 管线支持中文、日文、韩文输入法。
- 支持多个独立 WebView 实例。
- 支持导航、标题/URL/加载事件、鼠标输入和 DevTools。
- 支持 Dart 执行 JavaScript、JavaScript 调用 Dart。
- 支持 Cookie 管理和文档用户脚本注入。

## 环境要求

- Windows 10 或更高版本，x64。
- Flutter >= 3.27.0，Dart >= 3.6.0。
- 安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载。
- CMake 3.14 或更高版本。
- 支持 C++20 的 MSVC 工具链。

## 集成插件

在已有的 Flutter Windows 工程中执行：

```powershell
flutter pub add webview_cef
flutter config --enable-windows-desktop
flutter pub get
```

插件通过 `WebviewCefPluginCApi` 注册 Windows 插件，不需要任何 macOS、Linux 或 eLinux 配置。

## 集成 Windows Runner

CEF 使用独立的 renderer、GPU 和 utility 子进程。必须在 Flutter 创建主窗口之前初始化 CEF。在 `windows/runner/main.cpp` 中引入 C API 头文件，并在 `wWinMain` 的第一段代码调用 `initCEFProcesses`：

```cpp
#include "webview_cef/webview_cef_plugin_c_api.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev,
                      _In_ wchar_t* command_line, _In_ int show_command) {
  const int exit_code = initCEFProcesses(instance);
  if (exit_code >= 0) {
    return exit_code;
  }

  // 继续执行原有的 Flutter runner 初始化。
}
```

在 runner 的 Windows 消息循环中，`DispatchMessage` 后调用 `handleWndProcForCEF`。这一步用于处理键盘输入，并让 CEF 能够向 Flutter 引擎线程投递消息：

```cpp
MSG message;
while (GetMessage(&message, nullptr, 0, 0)) {
  TranslateMessage(&message);
  DispatchMessage(&message);
  handleWndProcForCEF(message.hwnd, message.message,
                      message.wParam, message.lParam);
}
```

插件会自动在 Flutter 窗口上拦截 `WM_IME_*` 消息，中文输入法不需要额外修改 runner。

## CEF 下载与构建

第一次执行 Windows 构建时，`windows/CMakeLists.txt` 会通过 `third/download.cmake` 将指定版本的官方 CEF Standard Distribution 下载到 `third/cef`，然后从源码编译 `libcef_dll_wrapper`。CEF 下载包约 330 MB，首次构建会明显较慢。

```powershell
flutter pub get
flutter build windows --debug
```

CEF 版本由 `third/download.cmake` 中的 `CEF_VERSION` 指定。升级 CEF 时修改该值并重新构建，不需要手动复制 CEF 文件。

## 运行时文件

Windows CMake 目标会将 CEF 运行时文件复制到 Flutter 可执行文件旁边，主要包括：

- `libcef.dll`
- CEF 的 `.pak` 资源文件
- `locales/` 目录
- CEF 提供的 ANGLE 和 SwiftShader 图形运行时文件

发布应用时必须复制完整的生成目录。只复制 Flutter exe 会导致 WebView 黑屏或 CEF 启动失败。

## Flutter 使用示例

整个应用只初始化一次 `WebviewManager`，然后创建控制器并初始化 URL：

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
    await controller.initialize('https://downloadcdn.oopz.cn/video_test_20260908/index3.html?debug=true');
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

整个应用退出时，在所有控制器销毁后调用一次 `WebviewManager().quit()`。

## 常用 API

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

JavaScript 调用 Dart 的示例：

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

## Windows 构建选项

以下选项定义在 `windows/CMakeLists.txt` 中：

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `WEBVIEW_CEF_GPU_TEXTURE` | `ON` | 使用 CEF D3D11 共享纹理和 Flutter GPU 纹理。设为 `OFF` 可切换到 CPU 像素缓冲路径。 |
| `WEBVIEW_CEF_USE_DEBUG_CEF` | `OFF` | 链接 CEF Debug 二进制。只有在需要调试 CEF 本身时才建议开启。 |

## 示例工程

仓库中的 [`example/`](example/) 是完整示例，运行方式：

```powershell
cd example
flutter pub get
flutter run -d windows
```

## 常见问题

- **CEF 重复下载**：删除不完整的 `third/cef` 目录，并确保网络正常后重新构建。
- **发布后 WebView 黑屏**：确认 exe 旁边存在 `libcef.dll`、`.pak` 文件、`locales/` 和图形运行时文件。
- **键盘输入无效**：确认 runner 在 `DispatchMessage` 后调用了 `handleWndProcForCEF`。
- **CEF 子进程没有正确启动**：确认 `initCEFProcesses(instance)` 是 `wWinMain` 中最先执行的逻辑，并立即返回非负返回值。
- **GPU 渲染不可用**：将 `WEBVIEW_CEF_GPU_TEXTURE` 设为 `OFF`，验证 CPU 回退路径是否正常。

## 许可证

[Apache License 2.0](LICENSE)
