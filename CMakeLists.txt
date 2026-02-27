cmake_minimum_required(VERSION 3.16)

project(FocusLock VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 确保字符集处理正确
if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# 资源文件和清单文件
set(RESOURCE_FILES
    resource.rc
    app.manifest
)

# 源文件列表
set(SOURCES
    main.cpp
    ui_setup.cpp
    ui_overlay.cpp
    ui_unlock.cpp
    overlay_engine.cpp
    security.cpp
    config.cpp
    svg_icons.cpp
    timer_engine.cpp
    tray.cpp
)

# 头文件列表
set(HEADERS
    common.h
    ui_setup.h
    ui_overlay.h
    ui_unlock.h
    overlay_engine.h
    security.h
    config.h
    svg_icons.h
    timer_engine.h
    tray.h
)

# 可执行文件
add_executable(FocusLock WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCE_FILES}
)

# 链接库 (移除 .lib 后缀以提高跨编译器兼容性)
target_link_libraries(FocusLock PRIVATE
    user32
    gdi32
    gdiplus
    shell32
    advapi32
    dwmapi
    ole32
    comctl32
    winmm
    bcrypt
    crypt32
)

# 设置清单文件属性（针对 MSVC）
if(MSVC)
    set_target_properties(FocusLock PROPERTIES LINK_FLAGS "/MANIFESTINPUT:\"${CMAKE_CURRENT_SOURCE_DIR}/app.manifest\"")
endif()
