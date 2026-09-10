/// 由运行在 [WebView] 中的 JavaScript 代码发送的消息。
class JavascriptMessage {
  /// 构造一个 JavaScript 消息对象。
  ///
  /// `message` 参数不能为 null。
  const JavascriptMessage(this.message, this.callbackId, this.frameId);

  /// 由 JavaScript 代码发送的消息内容。
  final String message;

  //  JavaScript 代码的 callbackId
  final String callbackId;
  //  webview 帧的 frameId
  final String frameId;
}

final RegExp _validChannelNames = RegExp('^[a-zA-Z_][a-zA-Z0-9.]*\$');

/// 用于接收来自 webview 内运行的 JavaScript 代码消息的命名通道。
class JavascriptChannel {
  /// 构造一个 Javascript 通道。
  ///
  /// `name` 和 `onMessageReceived` 参数不能为 null。
  JavascriptChannel({
    required this.name,
    required this.onMessageReceived,
  }) : assert(_validChannelNames.hasMatch(name));

  /// 通道的名称。
  ///
  /// 将此通道对象作为 [WebView.javascriptChannels] 的一部分传入，会向 Javascript window 对象
  /// 添加一个名为 `name` 的属性。
  ///
  /// 名称必须以字母或下划线(_)开头，后面可以是这些字符与数字的任意组合。
  ///
  /// 注意，任何与此名称相同的 JavaScript 已存在的 `window` 属性将被覆盖。
  ///
  /// 另请参阅 [WebView.javascriptChannels] 了解通道注册机制的更多详细信息。
  final String name;

  /// 当通过通道接收到消息时调用的回调。
  final JavascriptMessageHandler onMessageReceived;
}

/// 用于处理从 webview 中运行的 Javascript 发送的消息的回调类型。
typedef JavascriptMessageHandler = void Function(JavascriptMessage message);
