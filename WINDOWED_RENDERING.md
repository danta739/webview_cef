# 屏上渲染模式 (Windowed Mode) 使用说明

> 本文档介绍 `webview_cef` 插件的**屏上渲染(Windowed Mode)**模式。该模式与默认的离屏渲染(OSR)路径并存,允许在 Flutter 主窗口上叠加一个真实的 CEF 子窗口。

---

## 目录

1. [什么是屏上渲染](#什么是屏上渲染)
2. [架构概览](#架构概览)
3. [与离屏渲染的对比](#与离屏渲染的对比)
4. [文件结构](#文件结构)
5. [C++ 侧 API](#c-侧-api)
6. [Dart 侧 API](#dart-侧-api)
7. [Dart 端使用示例](#dart-端使用示例)
8. [位置/大小同步机制](#位置大小同步机制)
9. [运行示例](#运行示例)
10. [已知限制](#已知限制)
11. [FAQ](#faq)

---

## 什么是屏上渲染

屏上渲染(Windowed Mode,又称 On-screen Rendering)是 CEF 的三种渲染模式之一,与离屏渲染(OSR,Off-screen Rendering)相对。

| 概念 | 屏上渲染 | 离屏渲染(本插件默认) |
|---|---|---|
| 是否有原生 HWND | ✅ 有,真实 CEF 子窗口 | ❌ 无,渲染到内存/纹理 |
| 像素搬运 | 0 次(Chromium 直接合成) | 1 次(GPU 共享纹理)或 2 次(CPU 位图) |
| 集成到 Flutter | 当作"外部 HWND"叠加在窗口上 | 当作 `Texture` 嵌入 widget 树 |
| 适用场景 | 浏览器外壳、多窗口、高 DPI 标注 | 单页嵌入、性能优先 |

本插件默认采用**离屏 + GPU 共享纹理**,通过额外启用的屏上渲染模式,你可以在同一进程内同时拥有 OSR 和 Windowed 两套路径。

---

## 架构概览

```
┌──────────────────────────────────────────────────────────────┐
│  Flutter 主窗口 (HWND)                                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Flutter Widget 树                                       │   │
│  │   ├─ 工具栏 (Material widgets)                          │   │
│  │   ├─ 地址栏 (TextField)                                  │   │
│  │   └─ OSR WebView (Texture) — 离屏渲染                    │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  CEF 子窗口 (HWND) — 屏上渲染                            │   │
│  │   └─ Chromium 渲染进程直接合成到此 HWND                 │   │
│  │   └─ 通过 WndProc Hook 跟随宿主窗口 resize/move/DPI    │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
       ▲                ▲                ▲                ▲
       │                │                │                │
       │ WndProc        │ MethodChannel  │ GPU 共享纹理     │ MethodChannel
       │ Hook           │ "webview_cef"  │ (D3D11)         │ "webview_cef_windowed"
       │                │                │                │
   ┌───┴────┐       ┌───┴────┐       ┌───┴────┐       ┌───┴────┐
   │ Parent │       │ OSR   │       │ CEF   │       │ Windowed│
   │ Binding│       │ Plugin│       │ Browser│       │ Plugin │
   └────────┘       └────────┘       └────────┘       └────────┘
       common/webview_windowed.cc                       windows/webview_cef_windowed_plugin.cpp
```

### 三层职责划分

| 层 | 文件 | 职责 |
|---|---|---|
| **窗口同步层** | [common/webview_windowed.cc](common/webview_windowed.cc) | WndProc Hook、MoveWindow 同步 |
| **Flutter 桥接层** | [windows/webview_cef_windowed_plugin.cpp](windows/webview_cef_windowed_plugin.cpp) | MethodChannel 接收 Dart 调用 |
| **Dart API 层** | [lib/src/webview_windowed.dart](lib/src/webview_windowed.dart) | 用户调用的 Flutter 接口 |

---

## 与离屏渲染的对比

| 维度 | 屏上渲染 | 离屏渲染(OSR GPU 共享) |
|---|---|---|
| **CEF API 调用** | `window_info.SetAsChild(parent, rect)` | `window_info.SetAsWindowless(0)` + `shared_texture_enabled = true` |
| **回调** | 通过 `CefClient`(暂无,需自实现) | `OnAcceleratedPaint`(共享 HANDLE) |
| **像素传输** | 0 次,Chromium 直接合成 | 1 次 `CopyResource` |
| **Z-order** | 需手动控制 | 通过 Flutter widget 树 |
| **DPI 同步** | 通过 `WM_DPICHANGED` 自动 | 通过 `GetScreenInfo` |
| **键盘/鼠标路由** | 由 CEF 子窗口直接接收 | 由 Flutter widget 接收再注入 |
| **集成代码量** | 较多(窗口管理) | 较少(纯纹理) |
| **本插件支持** | ✅ 新增 | ✅ 默认 |

---

## 文件结构

本模式由以下文件组成(均为新增,**未修改任何 OSR 路径文件**):

| 文件 | 作用 |
|---|---|
| `common/webview_windowed.h` | 屏上渲染模块公共接口 |
| `common/webview_windowed.cc` | WndProc Hook、CEF 子窗口管理实现 |
| `windows/webview_cef_windowed_plugin.h` | Flutter 端屏上渲染插件声明 |
| `windows/webview_cef_windowed_plugin.cpp` | Flutter MethodChannel 处理实现 |
| `lib/src/webview_windowed.dart` | Dart 端 `WindowedWebviewManager` |
| `windows/CMakeLists.txt` | 注册新 .cc 文件(仅追加 4 行) |
| `windows/webview_cef_plugin_c_api.cpp` | 在 C API 导出时同时注册屏上渲染插件 |
| `lib/webview_cef.dart` | 导出 `webview_windowed.dart`(仅追加 1 行) |

---

## C++ 侧 API

### `webview_windowed::BindParentWindow`

```cpp
void BindParentWindow(HWND parent_hwnd);
```

将宿主窗口(Flutter 主窗口 HWND)绑定到屏上渲染模块。绑定后:

- 替换宿主窗口的 `WndProc`,拦截 `WM_SIZE` / `WM_MOVE` / `WM_DPICHANGED` / `WM_DESTROY`
- 这些消息触发时,会异步调用 `SyncChildRect` 让 CEF 子窗口跟随宿主窗口

**调用线程**:任意线程(线程安全)。通常在 Flutter 插件注册时调用一次。

### `webview_windowed::CreateWindowedBrowser`

```cpp
CefRefPtr<CefBrowser> CreateWindowedBrowser(
    HWND parent_hwnd,
    const CefString& url,
    const WindowedPlacement& placement);
```

在指定宿主窗口上创建一个 CEF 子窗口,加载给定 URL。

**参数**:

| 参数 | 说明 |
|---|---|
| `parent_hwnd` | 宿主窗口 HWND(Flutter 主窗口) |
| `url` | 初始加载地址 |
| `placement` | 子窗口初始位置和大小,相对于宿主客户区左上角 |

**调用线程**:必须在 CEF UI 线程(`TID_UI`)。

### `webview_windowed::SyncChildRect`

```cpp
void SyncChildRect(int browser_id);
```

将指定 browser 的 CEF 子窗口 rect 同步到宿主窗口的当前客户区(覆盖整个客户区)。

**调用线程**:必须在 CEF UI 线程。

### `webview_windowed::SyncChildRectTo`

```cpp
void SyncChildRectTo(int browser_id, int x, int y, int w, int h);
```

显式指定 CEF 子窗口的 rect(像素,相对父客户区)。

**调用线程**:必须在 CEF UI 线程。

### `webview_windowed::CloseWindowedBrowser`

```cpp
void CloseWindowedBrowser(int browser_id);
```

关闭并销毁指定 browser 的 CEF 子窗口。

**调用线程**:必须在 CEF UI 线程。

### `webview_windowed::IsWindowedModeBound`

```cpp
bool IsWindowedModeBound();
```

查询屏上渲染模式是否已绑定宿主窗口。

---

## Dart 侧 API

### `WindowedWebviewManager`

```dart
final manager = WindowedWebviewManager.instance;
```

全局单例,管理屏上渲染 webview 的创建、位置同步、销毁。

### `create`

```dart
Future<int> create({
  required String url,
  required WindowedRect rect,
});
```

创建一个屏上渲染 CEF 子窗口,返回 `browserId`。

| 参数 | 类型 | 说明 |
|---|---|---|
| `url` | `String` | 初始加载地址 |
| `rect` | `WindowedRect` | 子窗口初始位置和大小 |

### `updateRect`

```dart
Future<void> updateRect(WindowedRect rect);
```

同步子窗口的位置和大小(像素,相对父客户区)。

### `close`

```dart
Future<void> close();
```

关闭屏上渲染 webview 并释放底层 CEF 浏览器。

### `isBound`

```dart
Future<bool> isBound();
```

查询屏上渲染通道是否已绑定宿主窗口。

### `WindowedRect`

```dart
class WindowedRect {
  const WindowedRect({
    required this.x,
    required this.y,
    required this.width,
    required this.height,
  });

  final int x;       // 相对宿主客户区左上角的 X 偏移
  final int y;       // Y 偏移
  final int width;   // 宽度
  final int height;  // 高度
}
```

### 完整 API 表

| 方法 | MethodChannel 名 | 参数 | 返回 |
|---|---|---|---|
| `create` | `createWindowed` | `{url, x, y, w, h}` | `{browserId}` |
| `updateRect` | `updateWindowedRect` | `{x, y, w, h}` | `null` |
| `close` | `closeWindowed` | `null` | `null` |
| `isBound` | `isBound` | `null` | `{bound: bool}` |

---

## Dart 端使用示例

### 基础用法

```dart
import 'package:webview_cef/webview_cef.dart';

class MyApp extends StatefulWidget {
  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final _windowedManager = WindowedWebviewManager.instance;
  int? _browserId;

  @override
  void initState() {
    super.initState();
    _initWindowed();
  }

  Future<void> _initWindowed() async {
    // 1. 验证宿主窗口已绑定
    final bound = await _windowedManager.isBound();
    if (!bound) {
      print('Windowed mode not bound to parent window');
      return;
    }

    // 2. 创建屏上渲染 webview,显示在 (100,100) 位置,大小 1024×720
    _browserId = await _windowedManager.create(
      url: 'https://example.com',
      rect: const WindowedRect(x: 100, y: 100, width: 1024, height: 720),
    );
  }

  // 3. 动态调整位置(例如窗口 resize 后)
  Future<void> _onResize(int newWidth, int newHeight) async {
    if (_browserId == null) return;
    await _windowedManager.updateRect(
      WindowedRect(x: 0, y: 80, width: newWidth, height: newHeight - 80),
    );
  }

  @override
  void dispose() {
    _windowedManager.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: ElevatedButton(
          onPressed: () => _onResize(800, 600),
          child: const Text('调整到 800×600'),
        ),
      ),
    );
  }
}
```

### 与 OSR 共存

屏上渲染模式和 OSR 完全独立,可以在同一个 Flutter 窗口中同时存在:

```dart
class _MyAppState extends State<MyApp> {
  late WebViewController _osrController;  // OSR 控制器
  final _windowedManager = WindowedWebviewManager.instance;
  int? _windowedBrowserId;

  @override
  void initState() {
    super.initState();
    _initOsr();
    _initWindowed();
  }

  Future<void> _initOsr() async {
    // OSR 走现有 WebviewManager
    _osrController = WebviewManager().createWebView(
      loading: const Text('OSR loading...'),
    );
    await WebviewManager().initialize(userAgent: 'myapp');
    _osrController.setWebviewListener(WebviewEventsListener(
      onUrlChanged: (url) => debugPrint('OSR URL: $url'),
    ));
    await _osrController.initialize('https://flutter.dev');
  }

  Future<void> _initWindowed() async {
    _windowedBrowserId = await _windowedManager.create(
      url: 'https://pub.dev',
      rect: const WindowedRect(x: 200, y: 200, width: 800, height: 600),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        SizedBox(
          height: 400,
          child: _osrController.value
              ? _osrController.webviewWidget
              : _osrController.loadingWidget,
        ),
        // 屏上渲染的 CEF 子窗口直接叠加在 Flutter 主窗口上
        // 位置由 updateRect 控制,Flutter widget 树无需关心
        const Text('上方是 OSR,屏上渲染叠加在主窗口右侧'),
      ],
    );
  }
}
```

---

## 位置/大小同步机制

### 自动同步(宿主窗口事件)

屏上渲染模块在 `BindParentWindow` 时,会替换宿主窗口的 `WndProc`:

```cpp
// common/webview_windowed.cc
LRESULT CALLBACK ParentWndProcHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:        // 宿主 resize
        case WM_MOVE:        // 宿主 move
        case WM_DPICHANGED:  // DPI 变化(跨显示器)
            PostSyncChildRectTask(...);  // 异步同步所有 CEF 子窗口
            break;
        case WM_DESTROY:
            CloseAllWindowedBrowsers(true);
            break;
    }
    return ::CallWindowProc(orig_wndproc, ...);  // 调用原 WndProc
}
```

只要宿主窗口发生任何尺寸/位置变化,**所有**屏上渲染的 CEF 子窗口都会自动跟随。

### 手动同步(精确控制)

如果需要让屏上渲染子窗口只在宿主窗口的**部分区域**显示(例如浏览器外壳的地址栏下方的内容区),需要手动调用 `updateRect`:

```dart
// 在窗口 resize 后计算目标 rect 并同步
@override
void didChangeMetrics() {
  super.didChangeMetrics();
  final size = MediaQuery.of(context).size;
  // 屏上渲染子窗口铺满整个窗口客户区
  _windowedManager.updateRect(
    WindowedRect(x: 0, y: 0, width: size.width.toInt(), height: size.height.toInt()),
  );
}
```

### 同步原理

`updateRect` → MethodChannel → C++ 端 → `CefPostTask(TID_UI)` → `SyncChildRectTo` → `::MoveWindow` 子窗口 HWND。

所有 CEF UI 线程操作都通过 `CefPostTask` 异步派发,**调用方线程无需关心**线程安全。

---

## 运行示例

### 编译

```powershell
cd D:\workspace\webview_cef\example
flutter pub get
flutter build windows --debug
```

输出位于 `build\windows\x64\runner\Debug\webview_cef_example.exe`。

### 运行

```powershell
.\build\windows\x64\runner\Debug\webview_cef_example.exe
```

### 操作流程

1. 启动后默认显示 OSR 模式,加载默认 URL
2. 点击工具栏右侧的"打开外部"按钮(`Icons.open_in_new`)→ 切换到屏上渲染
3. CEF 子窗口叠加在 Flutter 主窗口 `(100,100)` 位置,大小 `1024×720`
4. 在屏上渲染界面可点击:
   - **调整到 (200,150) 800×600** → 测试 `updateRect` 同步
   - **还原初始位置** → 回到 `(100,100) 1024×720`
5. 拖动主窗口边缘 resize → 屏上渲染子窗口自动跟随(由 `WM_SIZE` hook 驱动)
6. 再次点击切换按钮(`Icons.layers_clear`)→ 关闭屏上渲染,恢复 OSR

---

## 已知限制

### 1. `CefClient` 未实现

当前 `CreateWindowedBrowser` 第二个参数传 `nullptr`,意味着:

| 缺失回调 | 影响 |
|---|---|
| `OnAddressChange` / `OnTitleChange` | 无法在 Flutter 端同步 URL/标题 |
| `OnLoadStart` / `OnLoadEnd` | 无法接收加载事件 |
| `OnBeforeClose` | 关闭事件无法通知到 Dart |

需要时需实现一个最小的 `CefClient` 子类,把这些回调桥接到现有 MethodChannel。

### 2. 输入事件路由缺失

屏上渲染模式下:

- 鼠标点击 CEF 子窗口 → Windows 直接派发到 CEF,Flutter 端收不到
- 键盘事件 → 同上
- 这是预期行为(屏上渲染的窗口本身就是独立的输入接收者)
- **如需从 Flutter UI 触发 webview 内部操作**(如点击 Flutter 按钮 → webview 内部 JS 执行),仍需走 OSR 路径或实现自定义 IPC

### 3. 仅支持单 tab

`WindowedWebviewPlugin` 当前只有一个 `active_browser_id_`:

```cpp
int active_browser_id_ = -1;
```

如果需要多 tab,需重构成:

```cpp
std::unordered_map<int, std::unique_ptr<...>> active_browsers_;
```

### 4. 屏上渲染的 CEF 子窗口会覆盖 Flutter widget

Z-order 由 `CreateWindow` 默认决定,**屏上渲染子窗口在 Flutter 控件之上**:

| 区域 | 显示内容 |
|---|---|
| 屏上渲染 rect 范围内 | CEF 渲染内容 |
| 其他区域 | 正常 Flutter widget |

如需 webview 只占部分区域:

```dart
Container(
  // 留出非 webview 区域给 Flutter UI
  width: 800,
  height: 600,
)
```

并配合 `updateRect` 让 CEF 子窗口只占对应区域。

### 5. DPI 处理依赖宿主窗口

`WM_DPICHANGED` 由宿主窗口 hook 接收,如果 Flutter 主窗口本身未启用 DPI 感知,屏上渲染也会受影响。建议在 `example.exe` 清单中声明 DPI 感知。

### 6. 多显示器下需手动同步

跨显示器拖动时,屏上渲染子窗口会跟随(`WM_MOVE` hook),但内部 CEF 渲染的 `device_scale_factor` 可能需要单独同步。

---

## FAQ

### Q1. 屏上渲染和 OSR 能同时用吗?

可以。两者使用不同的通道 (`webview_cef` vs `webview_cef_windowed`) 和不同的 CEF 浏览器实例。OSR 通过 Flutter Texture 显示,屏上渲染通过 CEF 子窗口显示。

### Q2. 切换屏上渲染/离屏会影响性能吗?

不会。每次切换都是:

- 创建/销毁 CEF 浏览器
- 注册/注销 Flutter Texture
- 创建/销毁 CEF 子窗口

没有持续性能开销,但每次切换需要几秒(取决于 CEF 启动 GPU 进程的时间)。

### Q3. 屏上渲染下能输入 URL 吗?

当前 demo 的地址栏只控制 OSR 的 URL。要让地址栏控制屏上渲染的 URL,需要在 `WindowedWebviewPlugin` 中新增 `loadUrl` 方法(类似 OSR 的 `_controller.loadUrl`),通过 IPC 调用 `browser->GetMainFrame()->LoadURL(...)`。

### Q4. 屏上渲染的 webview 能跟 Flutter 主题/暗色模式联动吗?

不能。屏上渲染是独立的 CEF 浏览器,渲染的是网页内容,不受 Flutter Material 主题影响。

### Q5. 屏上渲染和 OSR 的渲染速度差异?

- **OSR GPU 共享**:1 次 `CopyResource`(1080p 下 < 0.5ms)
- **屏上渲染**:0 次拷贝,但 Chromium 渲染开销略高(因为要合成到独立 HWND)

实际帧率取决于页面复杂度,两者都能轻松达到 60 FPS,屏上渲染在 120Hz/144Hz 显示器上表现略优。

### Q6. 怎样让屏上渲染的 webview 只显示在某个 Flutter widget 范围内?

```dart
LayoutBuilder(
  builder: (context, constraints) {
    // 计算 widget 在屏幕上的绝对坐标(屏幕坐标)
    final renderBox = context.findRenderObject() as RenderBox;
    final globalOffset = renderBox.localToGlobal(Offset.zero);

    WidgetsBinding.instance.addPostFrameCallback((_) {
      _windowedManager.updateRect(WindowedRect(
        x: globalOffset.dx.toInt(),
        y: globalOffset.dy.toInt(),
        width: constraints.maxWidth.toInt(),
        height: constraints.maxHeight.toInt(),
      ));
    });

    return Container(/* widget 主体 */);
  },
)
```

注意:`WindowedRect` 是相对宿主窗口客户区(0,0) 的坐标,不是屏幕坐标 —— **如果 Flutter 主窗口没有标题栏偏移,两者一致**;有标题栏时需要减去标题栏高度。

### Q7. 如何关闭 OSR,只保留屏上渲染?

在 `_controller` 创建后不要调用 `_controller.initialize(url)`,OSR 的 GPU 纹理就永远不会被创建。但 `WebviewManager().initialize()` 仍需调用,因为它会初始化 CEF 子系统。

### Q8. 屏上渲染模式下,Flutter UI 还能接收点击吗?

**屏上渲染子窗口范围内**:不能(被 CEF 拦截)
**其他 Flutter UI 区域**:能

如果需要让某个 Flutter widget 始终位于屏上渲染子窗口之上,可以用 `SetWindowPos` 把该 widget 的 HWND 提到 Z 序顶部(需要原生代码配合)。

### Q9. 如何调试屏上渲染的 CEF 渲染进程?

- 启动时加上 `--remote-debugging-port=9222`
- 浏览器访问 `chrome://inspect` 即可看到所有 CEF 渲染进程

需要在 [common/webview_app.cc](common/webview_app.cc) 的 `OnBeforeCommandLineProcessing` 中追加:

```cpp
command_line->AppendSwitchWithValue("remote-debugging-port", "9222");
```

### Q10. 屏上渲染支持 macOS/Linux 吗?

当前实现仅针对 Windows(使用 HWND / WndProc / MoveWindow)。macOS 的 NSView 和 Linux 的 X11/Wayland 需要单独实现,但 `webview_windowed.h` 的接口设计是平台无关的,可以扩展。

---

## 相关参考

- [CEF Off-Screen Rendering 文档](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage)
- [CEF Windowed Rendering 文档](同上)
- [Chromium GPU 进程架构](https://www.chromium.org/developers/design-documents/process-models)
- [Flutter PlatformView 文档](https://docs.flutter.dev/development/platform-integration/platform-views)

---

## 贡献与反馈

发现 bug 或有改进建议?请在 GitHub Issues 中提交,或在 PR 中直接修改本 README。

---

**最后更新**:2026-09-11