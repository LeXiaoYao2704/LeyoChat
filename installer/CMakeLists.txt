cmake_minimum_required(VERSION 3.24)

project(LeyoChatSetup VERSION ${CMAKE_PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Widgets)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

qt_add_executable(LeyoChatSetup WIN32
    main.cpp
    InstallerBackend.h
    InstallerLaunchPolicy.h
    InstallerBackend.cpp
    installer.rc
)

qt_add_qml_module(LeyoChatSetup
    URI LeyoChatInstaller
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/OptionsPage.qml
        qml/ProgressPage.qml
        qml/CompletePage.qml
        qml/showcase/ThemeSwitchDemo.qml
        qml/showcase/PageSlideDemo.qml
        qml/showcase/MessageBubbleDemo.qml
        qml/showcase/FileTransferDemo.qml
    RESOURCES
        resources/icon.png
)

target_link_libraries(LeyoChatSetup PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

# 将版本号从 CMake 注入 C++ 编译，避免手动同步
target_compile_definitions(LeyoChatSetup PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
)

# UAC manifest is embedded via installer.rc — no extra linker flags needed
if(MSVC)
    target_compile_options(LeyoChatSetup PRIVATE /utf-8)
    target_link_options(LeyoChatSetup PRIVATE /MANIFEST:NO)
endif()

# The Qt Quick installer shell is copied explicitly by package-installer.ps1.
# Keep it out of the default runtime install so client staging only contains
# the application runtime.
install(TARGETS LeyoChatSetup
    RUNTIME DESTINATION .
    COMPONENT InstallerShell
    EXCLUDE_FROM_ALL
)

# ── SFX Stub（微型自解压启动器，不依赖 Qt） ──────────────────────────────
add_executable(SfxStub WIN32 SfxStub.cpp sfxstub.rc)
target_link_libraries(SfxStub PRIVATE shlwapi)
if(MSVC)
    target_compile_options(SfxStub PRIVATE /utf-8)
    target_link_options(SfxStub PRIVATE /MANIFEST:NO)
    # 最小化体积
    target_compile_options(SfxStub PRIVATE /O1 /GS-)
endif()
set_target_properties(SfxStub PROPERTIES OUTPUT_NAME "SfxStub")
