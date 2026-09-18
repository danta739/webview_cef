// webview_value.h — 抽象值类型,与底层 CEF/Flutter 解耦。
//
// WValue 是一个引用计数的、可以持有 int / string / list / map 的值。
// 主要用于在 CEF 渲染进程与浏览器进程之间传递参数,避开 CEF 自带的
// CefValue 跨进程开销。

#ifndef WEBVIEW_VALUE_H
#define WEBVIEW_VALUE_H

// 防止 Windows.h 的 min/max 宏污染 CEF 模板代码。
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace webview_cef {

enum class WValueType {
    Null,
    Bool,
    Int,
    Double,
    String,
    List,
    Map,
};

class WValue;
using WValuePtr = WValue*;

class WValue {
 public:
    WValue();
    ~WValue();

    // 工厂方法
    static WValue* newNull();
    static WValue* newBool(bool v);
    static WValue* newInt(int64_t v);
    static WValue* newDouble(double v);
    static WValue* newString(const char* v);
    static WValue* newList();
    static WValue* newMap();

    void ref();
    void unref();

    WValueType type() const { return type_; }

    bool getBool() const;
    int64_t getInt() const;
    double getDouble() const;
    const char* getString() const;
    size_t getListSize() const;
    WValue* getListItem(size_t i) const;
    size_t getMapSize() const;
    WValue* getMapKey(size_t i) const;
    WValue* getMapValue(size_t i) const;
    WValue* getMapValue(const char* key) const;

    void setBool(bool v);
    void setInt(int64_t v);
    void setDouble(double v);
    void setString(const char* v);
    void appendList(WValue* v);
    void setMapValue(WValue* key, WValue* v);

 private:
    WValueType type_ = WValueType::Null;
    bool bool_ = false;
    int64_t int_ = 0;
    double double_ = 0.0;
    std::string string_;
    std::vector<WValue*> list_;
    std::vector<std::pair<WValue*, WValue*>> map_;  // 保持插入顺序
    int ref_count_ = 1;
};

inline WValue* webview_value_new_null() { return WValue::newNull(); }
inline WValue* webview_value_new_bool(bool v) { return WValue::newBool(v); }
inline WValue* webview_value_new_int(int64_t v) { return WValue::newInt(v); }
inline WValue* webview_value_new_double(double v) { return WValue::newDouble(v); }
inline WValue* webview_value_new_string(const char* v) { return WValue::newString(v); }
inline WValue* webview_value_new_list() { return WValue::newList(); }
inline WValue* webview_value_new_map() { return WValue::newMap(); }
inline void webview_value_ref(WValue* v) { if (v) v->ref(); }
inline void webview_value_unref(WValue* v) { if (v) v->unref(); }
inline WValueType webview_value_get_type(WValue* v) { return v ? v->type() : WValueType::Null; }
inline bool webview_value_get_bool(WValue* v) { return v ? v->getBool() : false; }
inline int64_t webview_value_get_int(WValue* v) { return v ? v->getInt() : 0; }
inline double webview_value_get_double(WValue* v) { return v ? v->getDouble() : 0.0; }
inline const char* webview_value_get_string(WValue* v) { return v ? v->getString() : ""; }
inline size_t webview_value_get_list_size(WValue* v) { return v ? v->getListSize() : 0; }
inline WValue* webview_value_get_list_item(WValue* v, size_t i) { return v ? v->getListItem(i) : nullptr; }
inline size_t webview_value_get_map_size(WValue* v) { return v ? v->getMapSize() : 0; }
inline WValue* webview_value_get_map_key(WValue* v, size_t i) { return v ? v->getMapKey(i) : nullptr; }
inline WValue* webview_value_get_map_value(WValue* v, size_t i) { return v ? v->getMapValue(i) : nullptr; }
inline WValue* webview_value_get_map_value(WValue* v, const char* k) { return v ? v->getMapValue(k) : nullptr; }
inline void webview_value_set_bool(WValue* v, bool val) { if (v) v->setBool(val); }
inline void webview_value_set_int(WValue* v, int64_t val) { if (v) v->setInt(val); }
inline void webview_value_set_double(WValue* v, double val) { if (v) v->setDouble(val); }
inline void webview_value_set_string(WValue* v, const char* val) { if (v) v->setString(val); }
inline void webview_value_append(WValue* v, WValue* item) { if (v) v->appendList(item); }
inline void webview_value_set(WValue* v, WValue* key, WValue* val) { if (v) v->setMapValue(key, val); }

}  // namespace webview_cef

#endif  // WEBVIEW_VALUE_H
