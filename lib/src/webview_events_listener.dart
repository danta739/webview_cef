import 'package:webview_cef/src/webview.dart';

typedef TitleChangeCb = void Function(String title);
typedef UrlChangeCb = void Function(String url);
/* 日志严重级别。来自 CEF include/internal/cef_types.h
  0:默认日志（当前为 info 级别）
  1:详细日志或调试日志
  2:info 日志
  3:warning 日志
  4:error 日志
  5:fatal 日志
  99:对所有消息禁用文件日志记录，并对严重程度低于 fatal 的消息禁用 stderr 输出
 */
typedef LoadStartCb = void Function(WebViewController controller, String url);
typedef LoadStopCb = void Function(WebViewController controller, String url);

typedef OnConsoleMessage = void Function(
    int level, String message, String source, int line);

class WebviewEventsListener {
  TitleChangeCb? onTitleChanged;
  UrlChangeCb? onUrlChanged;
  OnConsoleMessage? onConsoleMessage;
  LoadStartCb? onLoadStart;
  LoadStopCb? onLoadEnd;

  WebviewEventsListener({
    this.onTitleChanged,
    this.onUrlChanged,
    this.onConsoleMessage,
    this.onLoadStart,
    this.onLoadEnd,
  });
}
