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
    commentdialog.h commentdialog.cpp
    profiledialog.h profiledialog.cpp
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


# 回归测试直接编译业务类，通过本地 HTTP 服务检查真实 Qt 请求。
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(login_test tests/login_test.cpp login.cpp login.h login.ui
        apiclient.cpp apiclient.h datacenter.cpp datacenter.h imageRes.qrc)
    target_include_directories(login_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(login_test PRIVATE Qt::Core Qt::Widgets Qt::Network Qt::Test)
    add_test(NAME login COMMAND login_test -o ${CMAKE_CURRENT_BINARY_DIR}/login-results.txt,txt)
    set_tests_properties(login PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

    qt_add_executable(token_auth_test
        tests/token_auth_test.cpp apiclient.cpp apiclient.h datacenter.cpp datacenter.h)
    target_include_directories(token_auth_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(token_auth_test PRIVATE Qt::Core Qt::Network Qt::Test)
    # 结果写到构建目录，Windows 无控制台运行也能查看用例详情；不输出凭证。
    add_test(NAME token_auth COMMAND token_auth_test -o ${CMAKE_CURRENT_BINARY_DIR}/token-auth-results.txt,txt)

    # 页面使用可控播放替身和真实 HTTP 请求，验证加载顺序及窗口销毁边界。
    qt_add_executable(playerpage_test
        tests/playerpage_test.cpp playerpage.cpp playerpage.h playerpage.ui
        mpv/mpvplayer.h bulletscreenitem.cpp bulletscreenitem.h
        commentdialog.cpp commentdialog.h
        apiclient.cpp apiclient.h datacenter.cpp datacenter.h imageRes.qrc)
    target_include_directories(playerpage_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(playerpage_test PRIVATE Qt::Core Qt::Widgets Qt::Network Qt::Test)
    add_test(NAME playerpage COMMAND playerpage_test -o ${CMAKE_CURRENT_BINARY_DIR}/playerpage-results.txt,txt)
    set_tests_properties(playerpage PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(mpvplayer_test tests/mpvplayer_test.cpp mpv/mpvplayer.cpp mpv/mpvplayer.h)
    target_include_directories(mpvplayer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(mpvplayer_test PRIVATE Qt::Core Qt::Widgets Qt::Test)
    add_test(NAME mpvplayer COMMAND mpvplayer_test -o ${CMAKE_CURRENT_BINARY_DIR}/mpvplayer-results.txt,txt)
    set_tests_properties(token_auth playerpage mpvplayer PROPERTIES TIMEOUT 30)

    # 真实联调必须手动指定环境和开发账号，不加入默认 CTest，防止误写线上数据。
    qt_add_executable(live_ui_test tests/live_ui_test.cpp
        login.cpp login.h login.ui playerpage.cpp playerpage.h playerpage.ui
        apiclient.cpp apiclient.h datacenter.cpp datacenter.h
        mpv/mpvplayer.cpp mpv/mpvplayer.h bulletscreenitem.cpp bulletscreenitem.h
        commentdialog.cpp commentdialog.h imageRes.qrc)
    target_include_directories(live_ui_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(live_ui_test PRIVATE Qt::Core Qt::Widgets Qt::Network Qt::Test
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv.dll.a)
    add_custom_command(TARGET live_ui_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv-2.dll $<TARGET_FILE_DIR:live_ui_test>/libmpv-2.dll)

    qt_add_executable(live_backend_test tests/live_backend_test.cpp
        apiclient.cpp apiclient.h datacenter.cpp datacenter.h mpv/mpvplayer.cpp mpv/mpvplayer.h)
    target_include_directories(live_backend_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(live_backend_test PRIVATE Qt::Core Qt::Widgets Qt::Network Qt::Test
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv.dll.a)
    add_custom_command(TARGET live_backend_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mpv/dll/libmpv-2.dll $<TARGET_FILE_DIR:live_backend_test>/libmpv-2.dll)
endif()
