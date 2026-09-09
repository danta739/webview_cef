#include "webview_js_handler.h"
#include <atomic>

std::atomic_long s_nReqID {1001};

// ConvertCefV8ValueToJSValue
// 功能：将 CEF 转换为 JSV结构体
// 参数：value - CEF 的 V8 值对象，代表 JavaScript 中的一个值
// 返回：转换后的 JSValue 对象

JSValue ConvertCefV8ValueToJSValue(CefRefPtr<CefV8Value> value) {
    JSValue result;

    if (value->IsString()) {
        result.type = JSValue::Type::STRING;
        result.stringValue = value->GetStringValue().ToString();
    } else if (value->IsInt()) {
        result.type = JSValue::Type::INT;
        result.intValue = value->GetIntValue();
    } else if (value->IsBool()) {
        result.type = JSValue::Type::BOOL;
        result.boolValue = value->GetBoolValue();
    } else if (value->IsDouble()) {
        result.type = JSValue::Type::DOUBLE;
        result.doubleValue = value->GetDoubleValue();
    } else if (value->IsArray()) {
        result.type = JSValue::Type::ARRAY;
        int length = value->GetArrayLength();
        for (int i = 0; i < length; ++i) {
            result.arrayValue.push_back(ConvertCefV8ValueToJSValue(value->GetValue(i)));
        }
    } else {
        result.type = JSValue::Type::UNKNOWN;
    }

    return result;
}

// 功能：CEF 的 Js 调用入口函数
//       当 js 侧调用 NativeHost 对象的方法时，此函数会被触发（两种跨进程请求方式：jscmd，StartRequest）
// 参数：
//   name     - 被调用的 Js函数名
//   object   - 调用该函数的 JS 对象
//   arguments - 传入的参数列表
//   retval   - 返回值（输出参数）
//   exception - 异常信息（输出参数）
// 返回：true 表示处理完成，false 表示未处理
bool CefJSHandler::Execute(const CefString& name,
                           CefRefPtr<CefV8Value> object,
                           const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception)
{
    ///jsCmd 调用：JS 调用 C++ 函数（带回调）
    if (name == "jsCmd")
    {
        if (arguments.size() < 2) {
            exception = "Invalid arguments.";
            return true;
        }
        //the first param is function name,the last param is callback function,and allow most 2 custom params between them.
        CefString function_name = arguments[0]->GetStringValue();
        CefString params = "";
        CefRefPtr<CefV8Value> callback;
        CefRefPtr<CefV8Value> rawdata;
        ///jsCmd(functionName, callback)
        if (arguments[0]->IsString() && arguments[1]->IsFunction())
        {
            callback = arguments[1];
        }
        ///jsCmd(functionName, params, callback)
        else if (arguments[0]->IsString() && arguments[1]->IsString() && arguments[2]->IsFunction())
        {
            params = arguments[1]->GetStringValue();
            callback = arguments[2];
        }
        ///jsCmd(functionName, params, rawdata, callback)
        else if (arguments[0]->IsString() && arguments[1]->IsString() && arguments[3]->IsFunction())
        {
            params = arguments[1]->GetStringValue();
            rawdata = arguments[2];
            callback = arguments[3];
        }
        else
        {
            exception = "Invalid arguments.";
            return true;
        }

        //call c++ funtion
        if (!js_bridge_->CallCppFunction(function_name, params, callback, rawdata))
        {
            std::ostringstream strStream;
            strStream << "Failed to call function:  " << function_name.c_str() << ".";
            strStream.flush();

            exception = strStream.str();
        }

    }///网络请求：StartRequest(reqId, cmd, callback, args)
    else if (name == "StartRequest")
    {
        if (arguments.size() < 5) {
            exception = "Invalid arguments.";
            return true;
        }
        // args[0] - 请求 ID（会被取反后作为 key 存储回调）
        int reqId = (int)arguments[0]->GetIntValue();

        // args[1] - 命令字符串（如 URL、协议名等）
        CefString strCmd = arguments[1]->GetStringValue();

        //// args[2] - 回调函数名
        CefString strCallback = arguments[2]->GetStringValue();

        //// args[3] - 请求参数
        CefString strArgs = arguments[3]->GetStringValue();

        // call c++ function
        if (!js_bridge_->StartRequest(reqId, strCmd, strCallback, strArgs))
        {
            std::ostringstream strStream;
            strStream << "Failed to call function:  " << strCmd.c_str() << ".";
            strStream.flush();

            exception = strStream.str();
        }

    }
    else if (name == "GetNextReqID")
    {
        int reqID = CefJSBridge::GetNextReqID();
        retval = CefV8Value::CreateInt(reqID);
    }///从 C++ 侧向 JS 侧发送回调结果
    else if (name == "EvaluateCallback") {
        CefString callbackId = arguments[0]->GetStringValue();
        CefRefPtr<CefV8Value> result = arguments[1];
        JSValue jsValue = ConvertCefV8ValueToJSValue(result);

        if (!js_bridge_->EvaluateCallback(callbackId, jsValue)) {
            std::ostringstream strStream;
            strStream << "Failed to callback:  " << callbackId.c_str() << ".";
            strStream.flush();

            exception = strStream.str();
        }
    }
    else {
        exception = "NativeHost no this fun.";
    }

    return true;
}
/// <summary>
/// 发起跨进程请求（Render->Browser）
/// </summary>
/// <param name="reqId">请求 ID（负数表示 StartRequest 类型的请求）</param>
/// <param name="strCmd">命令字符串</param>
/// <param name="strCallback">JS 侧回调函数名</param>
/// <param name="strArgs">  请求参数</param>
/// <returns></returns>
bool CefJSBridge::StartRequest(int reqId,
                               const CefString& strCmd,
                               const CefString& strCallback,
                               const CefString& strArgs)
    ///请求标志位取反，保证为负数
{
    if (reqId > 0) {
        reqId *= -1;
    }
    ///  
    auto it = startRequest_callback_.find(reqId);
    if (it == startRequest_callback_.cend())
    {
        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        if (context)
        {
            CefRefPtr<CefFrame> frame = context->GetFrame();
            if (frame)
            { 
                // 创建跨进程消息，消息名为 kJSCallCppFunctionMessage
                CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(kJSCallCppFunctionMessage);
                message->GetArgumentList()->SetString(0, strCmd);
                message->GetArgumentList()->SetString(1, strArgs);
                message->GetArgumentList()->SetInt(2, reqId);
                // 将回调信息（frame + 回调函数名）以 reqId 为 key 保存
                startRequest_callback_.emplace(reqId, std::make_pair(frame, strCallback));
                // 发送消息到 Browser 进程
                frame->SendProcessMessage(PID_BROWSER, message);
                return true;
            }
        }
    }

    return false;
}
// 功能：将 C++ 侧的计算结果通过跨进程消息发送到 JS 侧执行回调
// 参数：
//   callbackId - 回调标识符（字符串形式）
//   result     - 要传递的结果数据（JSValue 类型）
// 返回：true 表示消息已发送，false 表示发送失败
// 说明：
//   1. 将 JSValue 的各个类型字转换为 CEF 的 CefListValue 格式
//   2. 通过 SendProcessMessage 发送到 Browser 进程

bool CefJSBridge::EvaluateCallback(const CefString& callbackId, const JSValue& result) {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    if (context) {
        CefRefPtr<CefFrame> frame = context->GetFrame();
        if (frame) {
            // 创建跨进程消息
            CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(
                                                                             kEvaluateCallbackMessage);
            CefRefPtr<CefListValue> args = message->GetArgumentList();

            args->SetString(0, callbackId);
           // 将数据设置到消息参数中

            switch (result.type) {
                case JSValue::Type::STRING:
                    args->SetString(1, result.stringValue);
                    break;
                case JSValue::Type::INT:
                    args->SetInt(1, result.intValue);
                    break;
                case JSValue::Type::BOOL:
                    args->SetBool(1, result.boolValue);
                    break;
                case JSValue::Type::DOUBLE:
                    args->SetDouble(1, result.doubleValue);
                    break;
                case JSValue::Type::ARRAY: {
                    // 数组类型：递归创建 CefListValue
                    CefRefPtr<CefListValue> arrayList = CefListValue::Create();
                    for (size_t i = 0; i < result.arrayValue.size(); ++i) {
                        const JSValue &element = result.arrayValue[i];
                        switch (element.type) {
                            case JSValue::Type::STRING:
                                arrayList->SetString(i, element.stringValue);
                                break;
                            case JSValue::Type::INT:
                                arrayList->SetInt(i, element.intValue);
                                break;
                            case JSValue::Type::BOOL:
                                arrayList->SetBool(i, element.boolValue);
                                break;
                            case JSValue::Type::DOUBLE:
                                arrayList->SetDouble(i, element.doubleValue);
                                break;
                            default:
                                break;
                        }
                    }
                    args->SetList(1, arrayList);
                    break;
                }
                default:
                    // For avoiding values like
                    // error: enumeration value 'UNKNOWN' not handled in switch [-Werror,-Wswitch]
                    break;
            }

            // 메시지 전송
            frame->SendProcessMessage(PID_BROWSER, message);
            return true;
        }
    }
    return false;
}
// CefJSBridge::GetNextReqID
// 功能：生成并返回一个递增的唯一请求 ID
// 返回：下一个请求 ID（始终为正整数）
//   1. 使用全局原子变量 s_nReqID 保证线程安全递增
//   2. 初始值为 1001
//   3. 如果递增后结果为 0（溢出回绕），则继续递增直到不为 0
int CefJSBridge::GetNextReqID()
{
    long nRet = ++s_nReqID;
    if (nRet < 0)
    {
        nRet = 0;
    }

    while (nRet == 0)
    {
        nRet = ++s_nReqID;
    }

    return nRet;
}
// 功能：从 JS 侧调用 C++ 函数（通过跨进程消息）
// args：
//   function_name - 要调用的 C++ 函数名
//   params        - 字符串参数
//   callback      - JS 侧回调函数（用于异步返回结果）
//   rawdata       - 原始数据（可选的额外参数）
// 返回：true 表示消息已发送，false 表示发送失败
//   1. 将回调信息（context + callback + rawdata）以 js_callback_id_ 为 key 保存
//   2. 创建跨进程消息发送到 Browser 进程
//   3. 当 Browser 进程处理完成后，通过 ExecuteJSCallbackFunc 执行 JS 回调

bool CefJSBridge::CallCppFunction(const CefString& function_name,
                                  const CefString& params,
                                  CefRefPtr<CefV8Value> callback,
                                  CefRefPtr<CefV8Value> rawdata)
{
    auto it = render_callback_.find(js_callback_id_);
    if (it == render_callback_.cend())
    {
        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        if (context)
        {
            CefRefPtr<CefFrame> frame = context->GetFrame();
            if (frame)
            {
                // 创建跨进程消息
                CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(kJSCallCppFunctionMessage);
                message->GetArgumentList()->SetString(0, function_name);
                message->GetArgumentList()->SetString(1, params);
                message->GetArgumentList()->SetInt(2, js_callback_id_);
                // 将回调信息保存，key 为当前回调 ID，value 为 (context, (callback, rawdata))
                render_callback_.emplace(js_callback_id_++, std::make_pair(context, std::make_pair(callback, rawdata)));
                frame->SendProcessMessage(PID_BROWSER, message);
                return true;
            }
        }
    }

    return false;
}

//   功能：当 Frame 被销毁时，移除与该 Frame 相关的所有待处理回调
//   args：frame - 即将被销毁的 CefFrame 对象
//   1. 遍历 render_callback_，移除与指定 frame 关联的回调
//   2. 遍历 startRequest_callback_，移除与指定 frame 关联的回调
//   3. 防止 Frame 销毁后回调被执行导致访问已释放内存

void CefJSBridge::RemoveCallbackFuncWithFrame(CefRefPtr<CefFrame> frame)
{
    ///清理render_callback_中与frame的回调
    if (!render_callback_.empty()) {
        for (auto it = render_callback_.begin(); it != render_callback_.end();) {
            //通过 v8 Context判断是否为对应frame
            if (it->second.first->IsSame(frame->GetV8Context())) {
                it = render_callback_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // 清理 startRequest_callback_ 中与指定 frame 相关的回调
    if (!startRequest_callback_.empty()) {
        for (auto it = startRequest_callback_.begin(); it != startRequest_callback_.end();) {
            // 通过 Frame Identifier 比较是否为同一个 frame
            if (it->second.first->GetIdentifier() == frame->GetIdentifier()) {
                it = startRequest_callback_.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

// 参数：
//   callbackId - 回调 ID（负数表示 StartRequest 类型，正数表示 jsCmd 类型）
//   error      - 是否发生错误
//   result     - 回调结果字符串
// 返回：true 表示回调已执行，false 表示未找到对应回调或执行失败
// 说明：
//   1. 根据 callbackId 的正负，分别从 startRequest_callback_ 或 render_callback_ 中查找
//   2. StartRequest 类型：通过 ExecuteJavaScript 直接在 JS 侧调用 window[callbackName]()
//   3. jsCmd 类型：通过 CefV8Value::ExecuteFunction 执行保存的 JS 回调函数
//      参数1: error (bool) 表示执行是否成功
//      参数2: result (string) 为执行结果
//      参数3 (可选): rawdata 原始数据

bool CefJSBridge::ExecuteJSCallbackFunc(int callbackId, bool error, const CefString& result)
{
    if (callbackId < 0)
    {
        auto it = startRequest_callback_.find(callbackId);
        if (it != startRequest_callback_.cend())
        {
            auto frame = it->second.first;
            CefString callback = it->second.second;

            if (callback != "" && frame.get())
            {
                std::ostringstream strStream;
                strStream <<"window['" << callback.ToString() << "'](" << callbackId * -1 << ", " << result.ToString() << ");";
                strStream.flush();

                CefString strCode = strStream.str();
                frame->ExecuteJavaScript(strCode, frame->GetURL(), 0);
                startRequest_callback_.erase(callbackId);

                return true;
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        auto it = render_callback_.find(callbackId);
        if (it != render_callback_.cend())
        {
            auto context = it->second.first;
            auto callback = it->second.second.first;
            auto rawdata = it->second.second.second;
            if (context.get() && callback.get())
            {
                context->Enter();

                CefV8ValueList arguments;

                //the first param marks whether the function execution result was successful
                arguments.push_back(CefV8Value::CreateBool(error));

                // the second prarm take the return data
                arguments.push_back(CefV8Value::CreateString(result));
                if (rawdata.get()) {
                    arguments.push_back(rawdata);
                }

                // call js function
                CefRefPtr<CefV8Value> retval = callback->ExecuteFunction(nullptr, arguments);
                context->Exit();
                render_callback_.erase(callbackId);

                return true;
            }
            else
            {
                return false;
            }
        }

    }

    return false;
}
