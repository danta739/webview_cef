// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "webview_app.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

WebviewApp::WebviewApp(CefRefPtr<WebviewHandler> handler) {
    m_handler = handler;
}

WebviewApp::ProcessType WebviewApp::GetProcessType(CefRefPtr<CefCommandLine> command_line)
{
    // 浏览器进程不会指定命令行标志。
	if (!command_line->HasSwitch("type"))
    {
        return BrowserProcess;
    }

	const std::string& process_type = command_line->GetSwitchValue("type");
    if (process_type == "renderer")
        return RendererProcess;
	return OtherProcess;
}

void WebviewApp::OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line)
{
    // 向浏览器进程传递其他命令行标志。
	if (process_type.empty())
	{
#ifndef WEBVIEW_CEF_GPU_TEXTURE
		// GPU 共享纹理路径（OnAcceleratedPaint）需要 GPU 合成器；
		// 仅在未编译此功能时才允许禁用 GPU。
		if (!m_bEnableGPU)
		{
			command_line->AppendSwitch("disable-gpu");
			command_line->AppendSwitch("disable-gpu-compositing");
		}
#endif

		command_line->AppendSwitch("disable-web-security");                                     //禁用 Web 安全
		command_line->AppendSwitch("allow-running-insecure-content");                           //允许在安全页面中运行不安全内容
		// 当未指定 cache-path 时，不创建 "GPUCache" 目录。
		command_line->AppendSwitch("disable-gpu-shader-disk-cache");                            //禁用 gpu 着色器磁盘缓存
        command_line->AppendSwitch("no-sandbox");

		//http://www.chromium.org/developers/design-documents/process-models
		if (m_uMode == 1)
		{
			command_line->AppendSwitch("process-per-site");                                     //每个站点运行在独立的进程中
			command_line->AppendSwitchWithValue("renderer-process-limit", "8");              //限制渲染进程数量以减少内存占用
		}
		else if (m_uMode == 2)
		{
			command_line->AppendSwitch("process-per-tab");                                      //每个标签运行在独立的进程中
		}
		else if (m_uMode == 3)
		{
			command_line->AppendSwitch("single-process");                                     //所有内容在同一个进程中
		}
		command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");     //媒体的自动播放策略

        //支持跨域请求
        std::string values = command_line->GetSwitchValue("disable-features");
        if (values == "")
        {
            values = "SameSiteByDefaultCookies,CookiesWithoutSameSiteMustBeSecure";
        }
        else
        {
            values += ",SameSiteByDefaultCookies,CookiesWithoutSameSiteMustBeSecure";
        }
        if (values.find("CalculateNativeWinOcclusion") == size_t(-1))
        {
            values += ",CalculateNativeWinOcclusion";
        }

        command_line->AppendSwitchWithValue("disable-features", values);
        // 对于不安全域名，将其加入白名单
		if (!m_strFilterDomain.empty())
		{
			command_line->AppendSwitch("ignore-certificate-errors");                            //忽略证书错误
			command_line->AppendSwitchWithValue("unsafely-treat-insecure-origin-as-secure",
                m_strFilterDomain);
		}
    }

}

void WebviewApp::OnContextInitialized()
{
    CEF_REQUIRE_UI_THREAD();
//    CefBrowserSettings browser_settings;
//    browser_settings.windowless_frame_rate = 60;
//                
//    CefWindowInfo window_info;
//    window_info.SetAsWindowless(0);
//
//    // create browser
//    CefBrowserHost::CreateBrowser(window_info, m_handler, "", browser_settings, nullptr, nullptr);
    
}

// CefRefPtr<CefClient> WebviewApp::GetDefaultClient() {
//     // 通过 Chrome 运行时 UI 创建新浏览器窗口时调用。
//     return WebviewHandler::GetInstance();
// }

void WebviewApp::SetUnSafelyTreatInsecureOriginAsSecure(const CefString &strFilterDomain)
{
    m_strFilterDomain = strFilterDomain;
}

void WebviewApp::OnWebKitInitialized()
{
    //为 jssdk 注入 js 函数
    std::string extensionCode = R"(
			var external = {};
			var clientSdk = {};
			(() => {
				clientSdk.jsCmd = (functionName, arg1, arg2, arg3) => {
					if (typeof arg1 === 'function') {
						native function jsCmd(functionName, arg1);
						return jsCmd(functionName, arg1);
					} 
					else if	 (typeof arg2 === 'function') {
                        jsonString = arg1;
                        if	(typeof arg1 !== 'string'){
						    jsonString = JSON.stringify(arg1);
                        }
						native function jsCmd(functionName, jsonString, arg2);
						return jsCmd(functionName, jsonString, arg2);
					}
					else if	 (typeof arg3 === 'function') {
                        jsonString = arg1;
                        if	(typeof arg1 !== 'string'){
						    jsonString = JSON.stringify(arg1);
                        }
						native function jsCmd(functionName, jsonString, arg2, arg3);
						return jsCmd(functionName, jsonString, arg2, arg3);
					}else {

					}
				};

                external.JavaScriptChannel = (n,e,r) => {
                    var a; 
                    null == r ? a = '' : (a = '_' + new Date + (1e3 + Math.floor(8999 * Math.random())), window[a] = function (n, e) { 
                        return function () { 
                            try {
                                e && e.call && e.call(null, arguments[1]) 
                            } finally {
                                delete window[n]
                            } 
                        } 
                    }(a, r)); 
                    try {
                        external.StartRequest(external.GetNextReqID(), n, a, JSON.stringify(e || {}), '') 
                    } catch (l) {
                        console.log('messeage send')
                    }
                }

                external.EvaluateCallback = (nReqID, result) => {
                    native function EvaluateCallback();
                    EvaluateCallback(nReqID, result);
                }

				external.StartRequest  = (nReqID, strCmd, strCallBack, strArgs, strLog) => {
					native function StartRequest();
					StartRequest(nReqID, strCmd, strCallBack, strArgs, strLog);
				};
				external.GetNextReqID  = () => {
				  native function GetNextReqID();
				  return GetNextReqID();
				};
			})();
		 )";

    CefRefPtr<CefJSHandler> handler = new CefJSHandler();

    if (!m_render_js_bridge.get())
        m_render_js_bridge.reset(new CefJSBridge);
    handler->AttachJSBridge(m_render_js_bridge);

    CefRegisterExtension("v8/extern", extensionCode, handler);
}

void WebviewApp::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> extra_info)
{
    if (!m_render_js_bridge.get()) {
        m_render_js_bridge.reset(new CefJSBridge);
    }
}

void WebviewApp::SetProcessMode(uint32_t uMode)
{
    m_uMode = uMode;
}

void WebviewApp::SetEnableGPU(bool bEnable)
{
    m_bEnableGPU = bEnable;
}

void WebviewApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line)
{
}

void WebviewApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
{
}

void WebviewApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
}

void WebviewApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    if (m_render_js_bridge.get())
    {
        m_render_js_bridge->RemoveCallbackFuncWithFrame(frame);
    }
}

void WebviewApp::OnUncaughtException(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Exception> exception, CefRefPtr<CefV8StackTrace> stackTrace)
{
}

void WebviewApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefDOMNode> node)
 {
    //获取节点属性
    bool is_editable = (node.get() && node->IsEditable());
    CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(kFocusedNodeChangedMessage);
    message->GetArgumentList()->SetBool(0, is_editable);
    if (is_editable)
    {
        CefRect rect = node->GetElementBounds();
        message->GetArgumentList()->SetInt(1, rect.x);
        message->GetArgumentList()->SetInt(2, rect.y + rect.height);
        message->GetArgumentList()->SetInt(3, rect.height);
    }
    frame->SendProcessMessage(PID_BROWSER, message);
}

bool WebviewApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    const CefString& message_name = message->GetName();
    if (message_name == kExecuteJsCallbackMessage)
    {
        int			callbackId = message->GetArgumentList()->GetInt(0);
        bool		error = message->GetArgumentList()->GetBool(1);
        CefString	result = message->GetArgumentList()->GetString(2);
        if (m_render_js_bridge.get())
        {
            m_render_js_bridge->ExecuteJSCallbackFunc(callbackId, error, result);
        }
    }

    return false;
}
