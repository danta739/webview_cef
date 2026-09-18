// 屏上渲染(Windowed Mode)模式 —— CEF 子 HWND 叠加到 Flutter 窗口之上。
//
// 设计原则:
//   * 不依赖 OSR 离屏渲染。CEF 直接合成到宿主窗口的子 HWND 上。
//   * 由 Dart 侧主动驱动几何信息(LayoutBuilder / WidgetsBindingObserver)
//     同步到 CEF 子窗口。
//   * 事件流通过 MethodChannel "webview_cef_windowed" 反向通知 Dart。
//
// 推荐用法:
//   WindowedWebviewManager.instance.setListener(
//     WindowedWebviewListener(
//       onUrlChanged: (url) => ...,
//       onTitleChanged: (title) => ...,
//       onLoadEnd: (url) => ...,
//       onConsoleMessage: (level, message, source, line) => ...,
//     ),
//   );
//   await WindowedWebviewManager.instance.create(url: 'https://example.com');
//
// 然后把 WindowedWebviewAutoSync 嵌入布局,Flutter 窗口 resize/DPI 变化时会自动
// 把 CEF 子窗口同步到当前可见区域。

import 'dart:async';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

/// WindowedRect:CEF 子窗口相对宿主窗口客户区的位置和大小(像素)。
class WindowedRect {
  const WindowedRect({
    required this.x,
    required this.y,
    required this.width,
    required this.height,
  });

  final int x;
  final int y;
  final int width;
  final int height;
}

/// WindowedWebviewListener:从 native 上报的事件回调集。
class WindowedWebviewListener {
  const WindowedWebviewListener({
    this.onUrlChanged,
    this.onTitleChanged,
    this.onLoadStart,
    this.onLoadEnd,
    this.onConsoleMessage,
    this.onBrowserClosed,
  });

  final void Function(String url)? onUrlChanged;
  final void Function(String title)? onTitleChanged;
  final void Function(String url)? onLoadStart;
  final void Function(String url)? onLoadEnd;
  final void Function(int level, String message, String source, int line)?
      onConsoleMessage;
  final void Function()? onBrowserClosed;
}

/// WindowedWebviewManager:进程级单例,管理 windowed 渲染模式。
class WindowedWebviewManager {
  WindowedWebviewManager._internal();

  static final WindowedWebviewManager instance =
      WindowedWebviewManager._internal();

  static const MethodChannel _channel =
      MethodChannel('webview_cef_windowed');

  int? _browserId;
  WindowedWebviewListener _listener = const WindowedWebviewListener();

  int? get browserId => _browserId;
  bool get isCreated => _browserId != null;

  /// 创建屏上渲染 CEF 子窗口。
  /// rect 传 0×0 时使用默认 800×600,避免 CEF 子窗口被创建为 0 大小而拒绝绘制。
  Future<int> create({
    required String url,
    WindowedRect rect =
        const WindowedRect(x: 0, y: 0, width: 800, height: 600),
  }) async {
    final result = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'createWindowed',
      {
        'url': url,
        'x': rect.x,
        'y': rect.y,
        'w': rect.width,
        'h': rect.height,
      },
    );
    final id = result?['browserId'] as int?;
    if (id == null) {
      throw StateError('createWindowed returned no browserId');
    }
    _browserId = id;
    return id;
  }

  /// 主动同步 CEF 子窗口的位置和大小(像素,父客户区坐标)。
  Future<void> updateRect(WindowedRect rect) async {
    if (_browserId == null) return;
    await _channel.invokeMethod('updateWindowedRect', {
      'x': rect.x,
      'y': rect.y,
      'w': rect.width,
      'h': rect.height,
    });
  }

  /// 由 Flutter 侧在宿主窗口 resize / DPI 改变时主动调用。
  Future<void> notifyResized() async {
    await _channel.invokeMethod('notifyResized');
  }

  Future<void> loadUrl(String url) async {
    if (_browserId == null) return;
    await _channel.invokeMethod('loadUrl', {'url': url});
  }

  Future<void> goBack() async =>
      _browserId == null ? Future.value() : _channel.invokeMethod('goBack');

  Future<void> goForward() async => _browserId == null
      ? Future.value()
      : _channel.invokeMethod('goForward');

  Future<void> reload() async =>
      _browserId == null ? Future.value() : _channel.invokeMethod('reload');

  Future<void> openDevTools() async => _browserId == null
      ? Future.value()
      : _channel.invokeMethod('openDevTools');

  /// 关闭 CEF 子窗口并释放资源。
  Future<void> close() async {
    if (_browserId == null) return;
    await _channel.invokeMethod('closeWindowed');
    _browserId = null;
  }

  /// 查询当前是否已绑定宿主窗口(屏上渲染模式生效中)。
  Future<bool> isBound() async {
    final r = await _channel.invokeMethod<Map<dynamic, dynamic>>('isBound');
    return (r?['bound'] as bool?) ?? false;
  }

  /// 设置事件监听器。
  void setListener(WindowedWebviewListener listener) {
    _listener = listener;
    _channel.setMethodCallHandler(_dispatch);
  }

  Future<dynamic> _dispatch(MethodCall call) async {
    final args = (call.arguments as Map?)?.cast<String, dynamic>() ?? {};
    switch (call.method) {
      case 'onUrlChanged':
        _listener.onUrlChanged?.call(args['url'] as String? ?? '');
        break;
      case 'onTitleChanged':
        _listener.onTitleChanged?.call(args['title'] as String? ?? '');
        break;
      case 'onLoadStart':
        _listener.onLoadStart?.call(args['url'] as String? ?? '');
        break;
      case 'onLoadEnd':
        _listener.onLoadEnd?.call(args['url'] as String? ?? '');
        break;
      case 'onConsoleMessage':
        _listener.onConsoleMessage?.call(
          (args['level'] as int?) ?? 0,
          args['message'] as String? ?? '',
          args['source'] as String? ?? '',
          (args['line'] as int?) ?? 0,
        );
        break;
      case 'onBrowserClosed':
        _listener.onBrowserClosed?.call();
        _browserId = null;
        break;
    }
    return null;
  }
}

/// WindowedWebviewAutoSync:把 WindowedWebviewManager 接入 Flutter WidgetsBinding +
/// LayoutBuilder,自动在宿主窗口 resize / DPI 变化 / 布局变化时把 CEF 子窗口同步
/// 到当前可见区域。
class WindowedWebviewAutoSync extends StatefulWidget {
  const WindowedWebviewAutoSync({
    super.key,
    required this.manager,
    required this.child,
  });

  final WindowedWebviewManager manager;
  final Widget child;

  @override
  State<WindowedWebviewAutoSync> createState() =>
      _WindowedWebviewAutoSyncState();
}

class _WindowedWebviewAutoSyncState extends State<WindowedWebviewAutoSync>
    with WidgetsBindingObserver {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeMetrics() {
    if (widget.manager.isCreated) {
      widget.manager.notifyResized();
    }
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final size = Size(constraints.maxWidth, constraints.maxHeight);
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!widget.manager.isCreated) return;
          if (size.width <= 0 || size.height <= 0) return;
          widget.manager.updateRect(WindowedRect(
            x: 0,
            y: 0,
            width: size.width.toInt(),
            height: size.height.toInt(),
          ));
        });
        return widget.child;
      },
    );
  }
}
