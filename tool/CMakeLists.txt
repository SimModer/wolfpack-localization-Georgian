cmake_minimum_required(VERSION 3.20)
project(WolfpackL10nTool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: a single portable exe, nothing to install.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7
  GIT_SHALLOW TRUE)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(imgui json)

add_executable(WolfpackL10n WIN32
  src/main.cpp src/ui.cpp src/util.cpp src/http.cpp src/game.cpp src/lang.cpp src/packs.cpp src/flags.cpp src/app.rc src/app.manifest
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)

target_include_directories(WolfpackL10n PRIVATE src third_party ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
target_link_libraries(WolfpackL10n PRIVATE nlohmann_json::nlohmann_json d3d11 dxgi winhttp bcrypt shell32 ole32 dwmapi comdlg32 advapi32)
target_compile_definitions(WolfpackL10n PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(WolfpackL10n PRIVATE /utf-8 /W3)
endif()
