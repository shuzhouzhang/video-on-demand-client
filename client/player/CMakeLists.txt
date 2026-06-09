# 这个文件是 player 客户端的 CMake 构建脚本。
# 它负责找到 Qt、收集源码/.ui/.qrc 文件，并生成最终的 player.exe。
cmake_minimum_required(VERSION 3.19)
project(player LANGUAGES CXX)

# 查找本项目需要用到的 Qt 模块。
# Core 是 Qt 基础模块，Widgets 提供 QWidget/QPushButton 等界面控件，Network 负责 HTTP 请求，Svg 用于 SVG 资源支持。
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Svg)

qt_standard_project_setup()

# 让 CMake 自动调用 Qt 的 rcc 工具编译 .qrc 资源文件。
# 否则代码里的 :/images/... 路径会找不到图片，按钮图标会显示为空白。
set(CMAKE_AUTORCC ON)

qt_add_executable(player
    WIN32 MACOSX_BUNDLE
    # 程序入口和主窗口。
    main.cpp
    player.cpp
    player.h
    player.ui

    # 启动页、自定义左侧导航按钮、资源文件和工具函数。
    startuppage.h startuppage.cpp
    imageRes.qrc
    apiclient.h apiclient.cpp
    pageswitchbutton.h pageswitchbutton.cpp
    videobox.h videobox.cpp videobox.ui
    datacenter.h datacenter.cpp
    bulletscreenitem.h bulletscreenitem.cpp
    playerpage.h playerpage.cpp playerpage.ui
    login.h login.cpp login.ui
    uploadvideopage.h uploadvideopage.cpp uploadvideopage.ui
    adminwidget.h adminwidget.cpp adminwidget.ui
    mpv/client.h
    mpv/mpvplayer.h mpv/mpvplayer.cpp
    util.h

)

target_include_directories(player
    PRIVATE
        # 让当前目录下的头文件可以直接通过 #include "xxx.h" 引入。
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 把项目代码和 Qt 库链接在一起，否则只编译源码还不能生成可运行程序。
target_link_libraries(player
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Svg
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv.dll.a
)

add_custom_command(TARGET player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv-2.dll
        $<TARGET_FILE_DIR:player>/libmpv-2.dll
    COMMENT "Copying libmpv runtime DLL"
)

if (WIN32)
    # Windows 平台窗口效果相关的系统库。
    target_link_libraries(player PRIVATE dwmapi)
endif()

install(TARGETS player
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET player
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
