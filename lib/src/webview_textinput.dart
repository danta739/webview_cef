import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

mixin WebeViewTextInput implements DeltaTextInputClient {
  @override
  TextEditingValue? currentTextEditingValue;

  @override
  AutofillScope? currentAutofillScope;

  // Windows 操作系统的 IME 由原生 WM_IME 管道驱动。Flutter 的
  // text-input/delta 路径不会为 OSR 浏览器提供组合输入 —— 留作占位。
  void attachTextInputClient() {}

  void detachTextInputClient() {}

  void updateIMEComposionPosition(double x, double y, double height, Offset offset) {}

  @override
  didChangeInputControl(
      TextInputControl? oldControl, TextInputControl? newControl) {
    debugPrint("changed");
  }

  @override
  connectionClosed() {}

  @override
  bool onFocusReceived() => false;

  @override
  insertTextPlaceholder(Size size) {}

  @override
  insertContent(KeyboardInsertedContent content) {}

  @override
  performAction(TextInputAction action) {}

  @override
  performPrivateCommand(String action, Map<String, dynamic> data) {}

  @override
  performSelector(String selectorName) {}

  @override
  removeTextPlaceholder() {}

  @override
  showAutocorrectionPromptRect(int start, int end) {}

  @override
  showToolbar() {}

  @override
  updateEditingValue(TextEditingValue value) {}

  @override
  updateFloatingCursor(RawFloatingCursorPoint point) {}
}
