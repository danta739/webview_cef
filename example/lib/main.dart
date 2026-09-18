// example/lib/main.dart — OSR + Windowed 双模式示例。
//
// 启动后默认走 OSR 离屏渲染(WebviewManager + WebViewController)。
// 通过 Windowed 切换按钮可以开关屏上渲染模式(在 Flutter 主窗口上叠加
// CEF 子 HWND)。

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:webview_cef/webview_cef.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  late WebViewController _controller;
  final _textController = TextEditingController();
  String title = "";
  Map allCookies = {};

  // 屏上渲染模式状态。
  bool _useWindowed = false;
  bool _windowedReady = false;
  int? _windowedBrowserId;

  // 屏上渲染初始 rect。实际项目中应根据窗口 resize / 布局动态调整。
  static const WindowedRect _kInitialRect =
      WindowedRect(x: 100, y: 100, width: 1024, height: 720);

  @override
  void initState() {
    InjectUserScripts injectUserScripts = InjectUserScripts();
    injectUserScripts.add(UserScript(
        "console.log('injectScript_in_LoadStart')",
        ScriptInjectTime.LOAD_START));
    injectUserScripts.add(UserScript(
        "console.log('injectScript_in_LoadEnd')", ScriptInjectTime.LOAD_END));

    _controller = WebviewManager().createWebView(
        loading: const Text("not initialized"),
        injectUserScripts: injectUserScripts);
    super.initState();
    initPlatformState();
  }

  @override
  void dispose() {
    if (_windowedReady) {
      WindowedWebviewManager.instance.close();
    }
    _controller.dispose();
    WebviewManager().quit();
    super.dispose();
  }

  Future<void> _enableWindowedMode(String url) async {
    if (_windowedReady) return;
    try {
      _windowedBrowserId = await WindowedWebviewManager.instance.create(
        url: url,
        rect: _kInitialRect,
      );
      setState(() {
        _windowedReady = true;
        _useWindowed = true;
      });
      debugPrint("Windowed webview created: id=$_windowedBrowserId");
    } catch (e) {
      debugPrint("Failed to create windowed webview: $e");
    }
  }

  Future<void> _disableWindowedMode() async {
    if (!_windowedReady) return;
    try {
      await WindowedWebviewManager.instance.close();
    } catch (e) {
      debugPrint("Failed to close windowed webview: $e");
    }
    setState(() {
      _windowedReady = false;
      _windowedBrowserId = null;
      _useWindowed = false;
    });
  }

  Future<void> _updateWindowedRect(WindowedRect rect) async {
    if (!_windowedReady) return;
    try {
      await WindowedWebviewManager.instance.updateRect(rect);
    } catch (e) {
      debugPrint("Failed to update windowed rect: $e");
    }
  }

  Future<void> initPlatformState() async {
    await WebviewManager().initialize(userAgent: "test/userAgent");
    String url =
        "https://downloadcdn.oopz.cn/video_test_20260908/index13.html?debug=true";
    _textController.text = url;

    _controller.setWebviewListener(WebviewEventsListener(
      onTitleChanged: (t) {
        setState(() {
          title = t;
        });
      },
      onUrlChanged: (url) {
        _textController.text = url;
      },
    ));

    await _controller.initialize(_textController.text);

    final bound = await WindowedWebviewManager.instance.isBound();
    debugPrint("WindowedWebviewManager bound = $bound");

    if (!mounted) return;
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData(useMaterial3: true),
      home: Scaffold(
        body: Column(
          children: [
            SizedBox(height: 20, child: Text(title)),
            Row(
              children: [
                SizedBox(
                  height: 48,
                  child: MaterialButton(
                    onPressed: () {
                      _controller.reload();
                    },
                    child: const Icon(Icons.refresh),
                  ),
                ),
                SizedBox(
                  height: 48,
                  child: MaterialButton(
                    onPressed: () {
                      _controller.goBack();
                    },
                    child: const Icon(Icons.arrow_left),
                  ),
                ),
                SizedBox(
                  height: 48,
                  child: MaterialButton(
                    onPressed: () {
                      _controller.goForward();
                    },
                    child: const Icon(Icons.arrow_right),
                  ),
                ),
                SizedBox(
                  height: 48,
                  child: MaterialButton(
                    onPressed: () async {
                      final url = _textController.text.isNotEmpty
                          ? _textController.text
                          : "https://downloadcdn.oopz.cn/video_test_20260908/index13.html?debug=true";
                      if (_windowedReady) {
                        await _disableWindowedMode();
                      } else {
                        await _enableWindowedMode(url);
                      }
                    },
                    child: Icon(_windowedReady
                        ? Icons.layers_clear
                        : Icons.open_in_new),
                  ),
                ),
                Expanded(
                  child: TextField(
                    controller: _textController,
                    onSubmitted: (url) {
                      _controller.loadUrl(url);
                    },
                  ),
                ),
              ],
            ),
            Expanded(
              child: Row(
                children: [
                  if (!_useWindowed)
                    ValueListenableBuilder(
                      valueListenable: _controller,
                      builder: (context, value, child) {
                        return _controller.value
                            ? Expanded(child: _controller.webviewWidget)
                            : _controller.loadingWidget;
                      },
                    )
                  else
                    Expanded(
                      child: Container(
                        color: Colors.grey.shade900,
                        child: Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              const Icon(Icons.open_in_new,
                                  size: 64, color: Colors.white70),
                              const SizedBox(height: 12),
                              const Text("Windowed 模式已启用",
                                  style: TextStyle(
                                      color: Colors.white, fontSize: 18)),
                              const SizedBox(height: 4),
                              Text(
                                "CEF 子窗口已叠加在主窗口 (${_kInitialRect.width}×${_kInitialRect.height} @ ${_kInitialRect.x},${_kInitialRect.y})",
                                style: const TextStyle(
                                    color: Colors.white60, fontSize: 12),
                              ),
                              if (_windowedBrowserId != null) ...[
                                const SizedBox(height: 8),
                                Text(
                                  "browserId = $_windowedBrowserId",
                                  style: const TextStyle(
                                      color: Colors.white38, fontSize: 11),
                                ),
                              ],
                              const SizedBox(height: 16),
                              ElevatedButton.icon(
                                onPressed: () => _updateWindowedRect(
                                  const WindowedRect(
                                      x: 200, y: 150, width: 800, height: 600),
                                ),
                                icon: const Icon(Icons.aspect_ratio),
                                label: const Text("调整到 (200,150) 800×600"),
                              ),
                              const SizedBox(height: 8),
                              ElevatedButton.icon(
                                onPressed: () => _updateWindowedRect(_kInitialRect),
                                icon: const Icon(Icons.refresh),
                                label: const Text("还原初始位置"),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
