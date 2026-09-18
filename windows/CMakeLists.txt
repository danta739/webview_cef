# webview_cef Windows plugin — 把 CEF 与 plugin bridge 编译进单一 DLL。

cmake_minimum_required(VERSION 3.14)

set(PLUGIN_NAME webview_cef)
set(PLUGIN_TARGET_NAME ${PLUGIN_NAME}_plugin)

if(DEFINED ENV{FLUTTER_WEBVIEW_CEF_ROOT})
  set(WEBVIEW_CEF_ROOT "$ENV{FLUTTER_WEBVIEW_CEF_ROOT}")
else()
  file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." _REAL_PARENT)
  set(WEBVIEW_CEF_ROOT "${_REAL_PARENT}")
endif()
# CMake 在 Windows 上把 \ 视为转义字符,file(GLOB) 返回的 source 路径
# 在 add_library(...) 中再次被解析时会把 \w 之类的报为 "Invalid escape"。
# 统一把所有路径变量强制转成 forward-slash 形式。
string(REPLACE "\\" "/" WEBVIEW_CEF_ROOT "${WEBVIEW_CEF_ROOT}")
set(CEF_ROOT "${WEBVIEW_CEF_ROOT}/third/cef")

if(DEFINED ENV{FLUTTER_EXAMPLE_DIR})
  set(EXAMPLE_DIR "$ENV{FLUTTER_EXAMPLE_DIR}")
else()
  # symlink 路径解析会让相对路径失败,因此默认尝试几个常见位置,
  # 但如果都不存在就报错提示用户设 FLUTTER_EXAMPLE_DIR。
  set(_candidate_example_dirs
    "${WEBVIEW_CEF_ROOT}/../example"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../example"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../example")
  set(EXAMPLE_DIR "")
  foreach(_cand IN LISTS _candidate_example_dirs)
    get_filename_component(_cand_abs "${_cand}" ABSOLUTE)
    if(EXISTS "${_cand_abs}/pubspec.yaml")
      set(EXAMPLE_DIR "${_cand_abs}")
      break()
    endif()
  endforeach()
endif()
# 同上,强制 forward-slash,避免 GLOB 源文件路径里残留的 \ 被 CMake 当 escape。
string(REPLACE "\\" "/" EXAMPLE_DIR "${EXAMPLE_DIR}")

if(NOT EXISTS "${EXAMPLE_DIR}")
  message(FATAL_ERROR "example dir not found at ${EXAMPLE_DIR}. Set FLUTTER_EXAMPLE_DIR env var.")
endif()

if(NOT EXISTS "${CEF_ROOT}/include/cef_version.h")
  message(FATAL_ERROR "CEF not found at ${CEF_ROOT}; run 'flutter build' first to download CEF")
endif()

# Flutter include 路径。flutter 的 native 头可能在多个位置:
#  - example/windows/flutter/ephemeral/cpp_client_wrapper/include  (flutter pub get 后)
#  - example/build/windows/x64/flutter/ephemeral/cpp_client_wrapper/include  (实际构建位置)
#  - FLUTTER_ROOT/packages/flutter_tools/.../client_wrapper/include  (Flutter SDK 内置)
set(_FLUTTER_WRAPPER_CANDIDATES
  "${EXAMPLE_DIR}/windows/flutter/ephemeral/cpp_client_wrapper/include"
  "${EXAMPLE_DIR}/build/windows/x64/flutter/ephemeral/cpp_client_wrapper/include"
  "$ENV{FLUTTER_ROOT}/packages/flutter_tools/lib/src/build_system/targets/windows/cpp_client_wrapper/include"
  # Flutter SDK 直接提供预生成的 engine artifacts (3.10+),无需 pub get 触发。
  "$ENV{FLUTTER_ROOT}/bin/cache/artifacts/engine/windows-x64/cpp_client_wrapper/include"
)
set(FLUTTER_CPP_WRAPPER_DIR "")
foreach(_cand IN LISTS _FLUTTER_WRAPPER_CANDIDATES)
  if(EXISTS "${_cand}/flutter/method_channel.h")
    set(FLUTTER_CPP_WRAPPER_DIR "${_cand}")
    break()
  endif()
endforeach()
if(NOT FLUTTER_CPP_WRAPPER_DIR)
  message(WARNING "Flutter cpp_client_wrapper not found in candidate paths; "
    "header lookups will fail. Set FLUTTER_EXAMPLE_DIR or run `flutter pub get` first.")
  set(FLUTTER_CPP_WRAPPER_DIR "${EXAMPLE_DIR}/windows/flutter/ephemeral/cpp_client_wrapper/include")
endif()

# FLUTTER_EPHEMERAL_DIR 同样采用 fallback:首选 example 下的 ephemeral,
# 缺失则用 build 目录下的。
if(EXISTS "${EXAMPLE_DIR}/windows/flutter/ephemeral/flutter_windows.h")
  set(FLUTTER_EPHEMERAL_DIR "${EXAMPLE_DIR}/windows/flutter/ephemeral")
elseif(EXISTS "${EXAMPLE_DIR}/build/windows/x64/flutter/ephemeral/flutter_windows.h")
  set(FLUTTER_EPHEMERAL_DIR "${EXAMPLE_DIR}/build/windows/x64/flutter/ephemeral")
elseif(EXISTS "$ENV{FLUTTER_ROOT}/bin/cache/artifacts/engine/windows-x64/flutter_windows.h")
  set(FLUTTER_EPHEMERAL_DIR "$ENV{FLUTTER_ROOT}/bin/cache/artifacts/engine/windows-x64")
else()
  set(FLUTTER_EPHEMERAL_DIR "${EXAMPLE_DIR}/windows/flutter/ephemeral")
endif()

message(STATUS "webview_cef root: ${WEBVIEW_CEF_ROOT}")
message(STATUS "CEF root: ${CEF_ROOT}")
message(STATUS "Example dir: ${EXAMPLE_DIR}")

set(WEBVIEW_CEF_COMMON_SOURCES
  ${WEBVIEW_CEF_ROOT}/common/webview_value.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_app.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_app_views.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_handler.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_js_handler.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_cookieVisitor.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_windowed.cc
  ${WEBVIEW_CEF_ROOT}/common/webview_plugin.cc
)

set(WEBVIEW_CEF_PLUGIN_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/webview_cef_plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/webview_cef_windowed_plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/webview_cef_plugin_c_api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/webview_cef_gpu_texture.cc
)

# libcef_dll 静态部分(logging / ref_counted / shutdown / transfer util /
# transfer_util / wrapper 等)在 CEF 149 标准分发中是源码形式,需要我们自己编译。
file(GLOB LIBCEF_DLL_SOURCES CONFIGURE_DEPENDS
  "${CEF_ROOT}/libcef_dll/base/*.cc"
  "${CEF_ROOT}/libcef_dll/cpptoc/*.cc"
  "${CEF_ROOT}/libcef_dll/ctocpp/*.cc"
  "${CEF_ROOT}/libcef_dll/wrapper/*.cc"
  "${CEF_ROOT}/libcef_dll/*.cc"
)

add_library(${PLUGIN_TARGET_NAME} SHARED
  ${WEBVIEW_CEF_COMMON_SOURCES}
  ${WEBVIEW_CEF_PLUGIN_SOURCES}
  ${LIBCEF_DLL_SOURCES}
)

# CEF 头用两种 include 风格:
#   - CEF 自带头写 #include "include/cef_X.h" → 需要 ${CEF_ROOT} 在 path 中
#   - 我们的代码写 #include "cef_X.h" 或 "include/cef_X.h" → 需要 ${CEF_ROOT} 和 ${CEF_ROOT}/include
target_include_directories(${PLUGIN_TARGET_NAME} PRIVATE
  ${CEF_ROOT}
  ${CEF_ROOT}/include
  ${CEF_ROOT}/libcef_dll
  ${FLUTTER_CPP_WRAPPER_DIR}
  ${FLUTTER_EPHEMERAL_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${WEBVIEW_CEF_ROOT}/common
)

target_compile_definitions(${PLUGIN_TARGET_NAME} PRIVATE
  USING_CEF_SHARED
  WRAPPING_CEF_SHARED
  CEF_INSIDE_PLUGIN
  WEBVIEW_CEF_GPU_TEXTURE
  UNICODE
  _UNICODE
  NOMINMAX
)

target_compile_features(${PLUGIN_TARGET_NAME} PRIVATE cxx_std_20)
# /FS 强制串行写 PDB —— libcef_dll 包含数百个 .cc,MSBuild /MP 并行时
# 多个 cl.exe 抢同一份 vc143.pdb 会触发 C1041。
target_compile_options(${PLUGIN_TARGET_NAME} PRIVATE /EHsc /FS /Z7)

if(EXISTS "${CEF_ROOT}/Release/libcef.lib")
  set(CEF_LIB_DIR "${CEF_ROOT}/Release")
elseif(EXISTS "${CEF_ROOT}/Debug/libcef.lib")
  set(CEF_LIB_DIR "${CEF_ROOT}/Debug")
endif()

if(CEF_LIB_DIR)
  target_link_libraries(${PLUGIN_TARGET_NAME} PRIVATE
    "${CEF_LIB_DIR}/libcef.lib"
  )
  target_link_directories(${PLUGIN_TARGET_NAME} PRIVATE "${CEF_LIB_DIR}")
endif()

# libcef_dll 内置的 cef_certificate_util_win.cc 等会用到 crypt32 / wintrust / version。
target_link_libraries(${PLUGIN_TARGET_NAME} PRIVATE
  crypt32
  wintrust
  version
)

target_link_libraries(${PLUGIN_TARGET_NAME} PRIVATE flutter_wrapper_plugin)
