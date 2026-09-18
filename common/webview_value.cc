// webview_value.cc — WValue 实现。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "webview_value.h"

namespace webview_cef {

WValue::WValue() = default;

WValue::~WValue() {
    for (auto* v : list_) v->unref();
    for (auto& kv : map_) {
        kv.first->unref();
        kv.second->unref();
    }
}

WValue* WValue::newNull() { return new WValue(); }
WValue* WValue::newBool(bool v) { auto* r = new WValue(); r->type_ = WValueType::Bool; r->bool_ = v; return r; }
WValue* WValue::newInt(int64_t v) { auto* r = new WValue(); r->type_ = WValueType::Int; r->int_ = v; return r; }
WValue* WValue::newDouble(double v) { auto* r = new WValue(); r->type_ = WValueType::Double; r->double_ = v; return r; }
WValue* WValue::newString(const char* v) {
    auto* r = new WValue();
    r->type_ = WValueType::String;
    if (v) r->string_ = v;
    return r;
}
WValue* WValue::newList() { auto* r = new WValue(); r->type_ = WValueType::List; return r; }
WValue* WValue::newMap() { auto* r = new WValue(); r->type_ = WValueType::Map; return r; }

void WValue::ref() { ++ref_count_; }
void WValue::unref() {
    if (--ref_count_ == 0) delete this;
}

bool WValue::getBool() const { return bool_; }
int64_t WValue::getInt() const { return int_; }
double WValue::getDouble() const { return double_; }
const char* WValue::getString() const { return string_.c_str(); }
size_t WValue::getListSize() const { return list_.size(); }
WValue* WValue::getListItem(size_t i) const { return i < list_.size() ? list_[i] : nullptr; }
size_t WValue::getMapSize() const { return map_.size(); }
WValue* WValue::getMapKey(size_t i) const { return i < map_.size() ? map_[i].first : nullptr; }
WValue* WValue::getMapValue(size_t i) const { return i < map_.size() ? map_[i].second : nullptr; }
WValue* WValue::getMapValue(const char* key) const {
    for (auto& kv : map_) {
        if (kv.first->type_ == WValueType::String && kv.first->string_ == key) {
            return kv.second;
        }
    }
    return nullptr;
}

void WValue::setBool(bool v) { type_ = WValueType::Bool; bool_ = v; }
void WValue::setInt(int64_t v) { type_ = WValueType::Int; int_ = v; }
void WValue::setDouble(double v) { type_ = WValueType::Double; double_ = v; }
void WValue::setString(const char* v) {
    type_ = WValueType::String;
    string_ = v ? v : "";
}

void WValue::appendList(WValue* v) {
    if (type_ != WValueType::List) type_ = WValueType::List;
    if (v) { v->ref(); list_.push_back(v); }
}

void WValue::setMapValue(WValue* key, WValue* v) {
    if (type_ != WValueType::Map) {
        type_ = WValueType::Map;
    }
    if (!key || !v) return;
    key->ref();
    v->ref();
    map_.emplace_back(key, v);
}

}  // namespace webview_cef
