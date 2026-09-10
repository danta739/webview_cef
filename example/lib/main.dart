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

  @override
  void initState() {
    //创建js脚本
    InjectUserScripts injectUserScripts = InjectUserScripts();
    // 注入一段 JS 脚本示例（在控制台打印日志）
    injectUserScripts.add(UserScript("console.log('injectScript_in_LoadStart')",
        ScriptInjectTime.LOAD_START));
    injectUserScripts.add(UserScript(
        "console.log('injectScript_in_LoadEnd')", ScriptInjectTime.LOAD_END));

    // 通过 WebviewManager 创建一个 WebView 实例
    // loading 参数指定 WebView 尚未初始化完成时显示的占位组件
    // injectUserScripts 参数传入需要注入的用户脚本配置
    _controller = WebviewManager().createWebView(
        loading: const Text("not initialized"),
        injectUserScripts: injectUserScripts);
    super.initState();
    initPlatformState();
  }

  @override
  void dispose() {
    _controller.dispose();
    WebviewManager().quit();
    super.dispose();
  }

  // 平台消息是异步的，所以我们在异步方法中进行初始化。
  Future<void> initPlatformState() async {
    // await WebviewManager().initialize(userAgent: "test/userAgent");
    await WebviewManager().initialize(userAgent: "video_test");
    String url = "https://downloadcdn.oopz.cn/video_test_20260908/index3.html?debug=true";
    _textController.text = url;
    //为所有平台设置用户代理的统一接口，处理回调
    _controller.setWebviewListener(WebviewEventsListener(
      onTitleChanged: (t) {
        setState(() {
          title = t;
        });
      },
      onUrlChanged: (url) {
        _textController.text = url;
        final Set<JavascriptChannel> jsChannels = {
          // 创建一个名为 'Print' 的 JS 通道
          JavascriptChannel(
              name: 'Print',
              onMessageReceived: (JavascriptMessage message) {
                debugPrint(message.message);
                _controller.sendJavaScriptChannelCallBack(
                    false,
                    "{'code':'200','message':'print succeed!'}",
                    message.callbackId,
                    message.frameId);
              }),
        };
        ///将 JS 通道注册到 WebView 示例：
         // 将 JS 通道注册到 WebView 控制器
        // _controller.setJavaScriptChannels(jsChannels);
        // //向 CEF 执行 JavaScript 代码来构建自己的 jssdk
        // _controller.executeJavaScript("function abc(e){return 'abc:'+ e}");
        // _controller
        //     .evaluateJavascript("abc('test')")
        //     .then((value) => debugPrint(value));
      },
      // onLoadStart: (controller, url) {
      //   debugPrint("onLoadStart => $url");
      // },
      // onLoadEnd: (controller, url) {
      //   debugPrint("onLoadEnd => $url");
      // },
    ));
    ///初始化CEF，加载页面
    await _controller.initialize(_textController.text);

    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
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
          SizedBox(
            height: 20,
            child: Text(title),
          ),
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
              // SizedBox(
              //   height: 48,
              //   child: MaterialButton(
              //     onPressed: () {
              //       _controller.openDevTools();
              //     },
              //     child: const Icon(Icons.developer_mode),
              //   ),
              // ),
              // Expanded(
              //   child: TextField(
              //     controller: _textController,
              //     onSubmitted: (url) {
              //       _controller.loadUrl(url);
              //       WebviewManager().visitAllCookies().then((value) {
              //         allCookies = Map.of(value);
              //         if (url == "baidu.com") {
              //           if (!allCookies.containsKey('.$url') ||
              //               !Map.of(allCookies['.$url']).containsKey('test')) {
              //             WebviewManager().setCookie(url, 'test', 'test123');
              //           } else {
              //             WebviewManager().deleteCookie(url, 'test');
              //           }
              //         }
              //       });
              //     },
              //   ),
              // ),
            ],
          ),
          Expanded(
              child: Row(
            children: [
              ValueListenableBuilder(
                valueListenable: _controller,
                builder: (context, value, child) {
                  return _controller.value
                      ? Expanded(child: _controller.webviewWidget)
                      : _controller.loadingWidget;
                },
              ),
            ],
          ))
        ],
      )),
    );
  }
}
