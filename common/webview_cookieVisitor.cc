#include "webview_cookieVisitor.h"

WebviewCookieVisitor::WebviewCookieVisitor()
{
}

WebviewCookieVisitor::~WebviewCookieVisitor()
{
}
void WebviewCookieVisitor::setOnVisitComplete(std::function<void(std::map<std::string, std::map<std::string, std::string>>)> complete)
{
    onVisitComplete = complete;
}
// CEF 回调函数：每遍历到一个 Cookie 就会被调用一次
// 参数说明：
//   cookie      - 当前遍历到的 Cookie 对象
//   count       - 当前是第几个（从 0 开始）
//   total       - Cookie 总数
//   deleteCookie - 输出参数，设为 true 则删除当前 Cookie
// 返回值：返回 true 继续遍历，返回 false 停止遍历
bool WebviewCookieVisitor::Visit(const CefCookie &cookie, int count, int total, bool &deleteCookie)
{
    {
        std::unique_lock<std::mutex> lock(m_mutexCookieVector);
	    if (count == 0)
	    {
		    m_vecAllCookies.clear();
	    }
        		
        m_vecAllCookies.emplace_back(cookie);
    }

    if(count == total - 1)
    {
        onVisitComplete(getVisitedCookies());
    }

    return count != total;
}
/// <summary>
/// cookie转为二维数组map
/// </summary>
/// <returns></returns>
std::map<std::string, std::map<std::string, std::string>> WebviewCookieVisitor::getVisitedCookies()
{
    std::map<std::string, std::map<std::string, std::string>> ret;
    for (auto &cookie : m_vecAllCookies)
    {
        ;
        std::string domain = CefString(cookie.domain.str).ToString();
        std::string name = CefString(cookie.name.str).ToString();
        std::string value = CefString(cookie.value.str).ToString();
        ret[domain][name] = value;
    }
    return ret;
}
