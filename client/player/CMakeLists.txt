cmake_minimum_required(VERSION 3.19)
project(player LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg)

qt_standard_project_setup()

# 让 CMake 自动调用 Qt 的 rcc 工具编译 .qrc 资源文件。
# 否则代码里的 :/images/... 路径会找不到图片，按钮图标会显示为空白。
set(CMAKE_AUTORCC ON)

qt_add_executable(player
    WIN32 MACOSX_BUNDLE
    main.cpp
    player.cpp
    player.h
    player.ui

    startuppage.h startuppage.cpp
    imageRes.qrc
    pageswitchbutton.h pageswitchbutton.cpp
    util.h

)

target_link_libraries(player
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Svg
)

if (WIN32)
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
