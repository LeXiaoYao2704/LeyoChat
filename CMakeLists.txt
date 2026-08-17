cmake_minimum_required(VERSION 3.24)

project(LeyoChat VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
include(CompilerWarnings)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql Test Concurrent Multimedia)
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
find_package(Qt6 QUIET COMPONENTS HttpServer)

if(Qt6WebEngineWidgets_FOUND)
    message(STATUS "Qt6::WebEngineWidgets found — online editor enabled")
else()
    message(STATUS "Qt6::WebEngineWidgets not found — online editor disabled (MinGW)")
endif()

if(Qt6HttpServer_FOUND)
    message(STATUS "Qt6::HttpServer found — LeyoChatService will be built")
else()
    message(STATUS "Qt6::HttpServer not found — LeyoChatService will NOT be built")
endif()

# ── 第三方库 ──────────────────────────────────────────────
add_subdirectory(third_party/md4c)
add_subdirectory(third_party/ElaWidgetTools)

enable_testing()

option(LEYOCHAT_BUILD_TESTS "Build LeyoChat test targets" OFF)

set(LEYOCHAT_APP_ID "{{4480A57B-AD7A-4723-9976-C6EDFBC4C2AD}}")
set(LEYOCHAT_COMPANY_NAME "LeXiaoYao2704")
set(LEYOCHAT_PRODUCT_NAME "LeyoChat")
set(LEYOCHAT_COPYRIGHT "Copyright (C) 2026 LeXiaoYao2704")
set(LEYOCHAT_FILE_DESCRIPTION "LeyoChat Desktop Client")
set(LEYOCHAT_EXECUTABLE_NAME "LeyoChat.exe")
set(LEYOCHAT_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/windows/leyochat-icon.ico")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LEYOCHAT_ICON_RC_PATH "${LEYOCHAT_ICON_PATH}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/AppBuildInfo.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/AppBuildInfo.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/LeyoChat.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/LeyoChat.rc"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/LeyoChatLauncher.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/LeyoChatLauncher.rc"
    @ONLY
)

add_executable(LeyoChat WIN32
    windows/leyochat.qrc
    src/app/main.cpp
    src/app/AboutDialogHelpers.cpp
    src/app/AppSettings.cpp
    src/app/ApplicationInfo.cpp
    src/app/AppPathHelpers.cpp
    src/app/AppResourceDiagnostics.cpp
    src/app/AttachmentPayloadHelpers.cpp
    src/app/AvatarStorageHelpers.cpp
    src/app/CallControlWireCodec.cpp
    src/app/ConversationSummaryHelpers.cpp
    src/app/DeliveryReceiptHelpers.cpp
    src/app/DocumentDialogHelpers.cpp
    src/app/FileOpenHelpers.cpp
    src/app/GroupFanOutDelivery.cpp
    src/app/GroupFanOutPayloadBuilder.cpp
    src/app/GroupFileServiceConfigResolver.cpp
    src/app/GroupFileSendRouting.cpp
    src/app/LeyoApplication.cpp
    src/app/ImageFileTypeHelpers.cpp
    src/app/IntegrationNotificationPresentation.cpp
    src/app/IntegrationPollBackoff.cpp
    src/app/LocalFileServiceAutostart.cpp
    src/app/MessagePresentationHelpers.cpp
    src/app/PeerPresentationHelpers.cpp
    src/app/ReminderItemFactory.cpp
    src/app/RemotePresenceUiAdapter.cpp
    src/app/SystemActivityHelpers.cpp
    src/app/TestModeContext.cpp
    src/app/TransferSpeedTracker.cpp
    src/app/UiRestoreHelpers.cpp
    src/recovery/ClientRecoveryState.cpp
    src/recovery/ClientStartupOptions.cpp
    src/architecture/ArchitectureSnapshotAssembler.cpp
    src/architecture/HybridRoutingPolicy.cpp
    src/architecture/DatabaseResourceCatalog.cpp
    src/architecture/DatabaseServiceBindingCatalog.cpp
    src/architecture/DatabaseServiceDiscoveryProvider.cpp
    src/architecture/DatabaseServiceSelectionCatalog.cpp
    src/architecture/PersistedRuntimeArchitectureLoader.cpp
    src/architecture/RuntimeArchitecturePresentation.cpp
    src/architecture/RuntimeArchitectureFacade.cpp
    src/architecture/RuntimeArchitectureQueryService.cpp
    src/architecture/ResourceReferenceMessage.cpp
    src/call/CallSession.cpp
    src/call/AudioChannel.cpp
    src/call/ScreenCaptureChannel.cpp
    src/call/ScreenViewerWidget.cpp
    src/call/RemoteInputChannel.cpp
    src/diagnostics/Diagnostics.cpp
    src/diagnostics/RuntimeDiagnostics.cpp
    src/network/FileTransferConnection.cpp
    src/network/FileTransferServer.cpp
    src/network/HttpFileClient.cpp
    src/network/MessageCodec.cpp
    src/network/PeerHandshake.cpp
    src/network/PeerConnection.cpp
    src/network/PeerServer.cpp
    src/network/PeerSessionManager.cpp
    src/network/TlsHelper.cpp
    src/network/LanDiscoveryService.cpp
    src/integrations/AzureDevOpsLinkParser.cpp
    src/integrations/AzureDevOpsNotificationContracts.cpp
    src/integrations/AzureDevOpsSettings.cpp
    src/integrations/DevOpsAdapterContracts.cpp
    src/integrations/LocalAzureDevOpsAdapter.cpp
    src/integrations/LocalOutlookAdapter.cpp
    src/integrations/OutlookAdapterContracts.cpp
    src/integrations/OutlookNotificationContracts.cpp
    src/integrations/OutlookEwsTransport.cpp
    src/integrations/OutlookSettings.cpp
    src/integrations/RemoteFileServiceAdapter.cpp
    src/integrations/RemoteChatServiceSettings.cpp
    src/integrations/RemoteFileServiceContracts.cpp
    src/integrations/RemoteFileServiceSettings.cpp
    src/integrations/ServerMessageClient.cpp
    src/integrations/SharedFileResourceContracts.cpp
    src/services/AzureDevOpsBuildNotificationPoller.cpp
    src/services/AzureDevOpsNotificationDispatcher.cpp
    src/services/ChatService.cpp
    src/services/MessageMutationService.h
    src/services/MessageMutationService.cpp
    src/services/DirectConversationAddressing.cpp
    src/services/FileReceiveWorker.cpp
    src/services/FileTransferService.cpp
    src/services/OutgoingFileSnapshot.cpp
    src/services/GroupFileTransferService.cpp
    src/services/GroupService.cpp
    src/services/MessageSyncService.cpp
    src/services/OutlookNotificationDispatcher.cpp
    src/services/OutlookNotificationPoller.cpp
    src/services/OutlookStreamingConnection.cpp
    src/services/P2PConnectionPolicy.cpp
    src/services/PeerDirectoryService.cpp
    src/services/RecipientCapabilityResolver.cpp
    src/services/ReminderService.cpp
    src/services/ReminderTimeOptions.cpp
    src/services/ReliableDirectEnvelopeSender.cpp
    src/services/ReliableDirectMessageSender.cpp
    src/services/ReliableGroupEnvelopeSender.cpp
    src/services/ReliableGroupFileMessageSender.cpp
    src/services/ReliableGroupMessageSender.cpp
    src/services/RemoteMessageDeviceIdentity.cpp
    src/services/RemoteMessageEventConsumer.cpp
    src/services/RemoteMessageSyncCoordinator.cpp
    src/services/SharedFileResourceSync.cpp
    src/services/TransportPolicy.cpp
    src/storage/ConversationRepository.cpp
    src/storage/FileTransferRepository.cpp
    src/storage/GroupRepository.cpp
    src/storage/ReminderRepository.cpp
    src/ui/AzureDevOpsInsertDialog.cpp
    src/ui/AzureDevOpsNotificationDialog.cpp
    src/ui/ReminderDialog.cpp
    src/ui/ReminderListModel.cpp
    src/ui/ChatComposerWidget.cpp
    src/ui/ChatHeaderWidget.cpp
    src/ui/ClientAppearance.cpp
    src/ui/ClientPreferences.cpp
    src/ui/CloseToTrayDialog.cpp
    src/ui/ConversationListModel.cpp
    src/ui/ConnectIpDialog.cpp
    src/ui/ConversationHistoryCalendarWidget.cpp
    src/ui/ConversationHistoryDialog.cpp
    src/ui/ConversationSidebarWidget.cpp
    src/ui/ChatWorkspaceWidget.cpp
    src/ui/CreateGroupDialog.cpp
    src/ui/ForwardDetailDialog.cpp
    src/ui/GroupInfoPanel.cpp
    src/ui/InitialSetupPage.cpp
    src/ui/LoginDialog.cpp
    src/ui/PinyinHelper.cpp
    src/ui/ProfileCardPopup.cpp
    src/ui/MainWindow.cpp
    src/ui/GlobalSearchPanel.cpp
    src/ui/GlobalSearchHistory.cpp
    src/ui/WorkbenchPage.cpp
    src/ui/CallWindow.cpp
    src/call/CallSoundPlayer.cpp
    src/ui/RuntimeArchitectureDialog.cpp
    src/ui/FileVersionHistoryDialog.cpp
    src/ui/MessageListModel.cpp
    src/ui/MessageDeliveryPresentation.cpp
    src/ui/OutlookNotificationDialog.cpp
    src/ui/SettingsPage.cpp
    src/ui/ContactSelectionDialog.cpp
    src/ui/StorageCleanupDialog.cpp
    src/ui/ConversationsPage.cpp
    src/ui/DirectoryPage.cpp
    src/ui/NotificationsPage.cpp
    src/ui/KnowledgePage.cpp
    src/ui/TransferListModel.cpp
    src/ui/ConversationItemWidget.cpp
    src/ui/ConversationCardDelegate.cpp
    src/ui/MessageBubbleWidget.cpp
    src/ui/MessageBubbleDelegate.cpp
    src/ui/MessageTextLinkifier.cpp
    src/ui/ReactionToolbar.cpp
    src/ui/MessageThumbnailCache.cpp
    src/ui/ContactCardDelegate.cpp
    src/ui/ContactListModel.cpp
    src/ui/AlphabetIndexBar.cpp
    src/ui/OrgTreeDelegate.cpp
    src/ui/ScreenshotEditorWindow.cpp
    src/ui/ScreenshotOverlay.cpp
    src/ui/GlobalHotkeyManager.cpp
    src/ui/GroupFileTableModel.cpp
    src/ui/GroupFileVersionDialog.cpp
    src/ui/GroupFileManagerDialog.cpp
    src/ui/FilePreviewWidget.cpp
    src/ui/StickerManager.cpp
    src/ui/ImageViewerWidget.cpp
    src/ui/MarkdownRenderer.cpp
    src/integrations/HttpKnowServiceClient.cpp
    src/integrations/KnowledgeServiceSettings.cpp
    src/ui/AiKnowledgePanel.cpp
    src/ui/KnowledgeServiceSettingsWidget.cpp
    src/services/IdentityService.cpp
    src/services/ResourceRefRouter.cpp
    src/storage/DatabaseManager.cpp
    src/storage/ProfileRepository.cpp
    src/storage/ServiceBindingRepository.cpp
    src/storage/ServiceRegistryRepository.cpp
    src/storage/ServiceResourceRepository.cpp
    src/store/ChatDataStore.cpp
    src/store/DatabaseWorker.cpp
    src/update/UpdateChecker.cpp
    src/update/UpdateDownloader.cpp
    src/ui/UpdateBar.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/LeyoChat.rc"
        src/ui/ContextPanel.cpp
)

if(Qt6HttpServer_FOUND)
set(LEYO_FILE_SERVICE_SOURCES
    src/fileservice/FileServiceAuth.cpp
    src/fileservice/FileServiceDatabase.cpp
    src/fileservice/FileServiceHttpServer.cpp
    src/fileservice/FileStorageManager.cpp
    src/fileservice/WopiHandler.cpp
    src/fileservice/OnlyOfficeCallbackHandler.cpp
)

add_executable(LeyoChatService
    src/chatservice/main.cpp
    src/chatservice/ServiceConfig.cpp
    ${LEYO_FILE_SERVICE_SOURCES}
    src/fileservice/MessageEventBus.cpp
    src/fileservice/MessageServiceDatabase.cpp
    src/fileservice/MessageServiceHttpContracts.cpp
    src/fileservice/MessageServiceOperations.cpp
    src/fileservice/MessageSessionRegistry.cpp
    src/fileservice/MessageServiceHttpRoutes.cpp
)

add_executable(LeyoChatServiceHost
    src/chatservice/ServiceHostMain.cpp
    src/chatservice/ServiceHostPolicy.cpp
)
endif()

target_include_directories(LeyoChat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
if(MSVC)
    target_compile_definitions(LeyoChat PRIVATE NOMINMAX)
    target_compile_options(LeyoChat PRIVATE /utf-8 /Zi)
    set_source_files_properties(src/app/LeyoApplication.cpp PROPERTIES COMPILE_OPTIONS /bigobj)
    target_link_options(LeyoChat PRIVATE /DEBUG)
endif()
target_link_libraries(LeyoChat PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Multimedia md4c ElaWidgetTools)
if(WIN32)
    target_link_libraries(LeyoChat PRIVATE psapi)
endif()
add_custom_command(TARGET LeyoChat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/branding"
        "$<TARGET_FILE_DIR:LeyoChat>/resources/branding"
)
if(Qt6WebEngineWidgets_FOUND)
    target_link_libraries(LeyoChat PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(LeyoChat PRIVATE LEYOCHAT_HAS_WEBENGINE)
    target_sources(LeyoChat PRIVATE
        src/ui/OnlineEditorWidget.cpp
        src/ui/FerryBrowserWidget.cpp
    )
endif()
enable_project_warnings(LeyoChat)
set_target_properties(LeyoChat PROPERTIES OUTPUT_NAME "LeyoChat")

if(WIN32)
add_executable(LeyoChatLauncher WIN32
    src/launcher/LauncherMain.cpp
    src/launcher/LauncherPolicy.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/LeyoChatLauncher.rc"
)
target_include_directories(LeyoChatLauncher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(LeyoChatLauncher PRIVATE NOMINMAX UNICODE _UNICODE)
if(MSVC)
    target_compile_options(LeyoChatLauncher PRIVATE /utf-8 /Zi)
    target_link_options(LeyoChatLauncher PRIVATE /DEBUG)
endif()
target_link_libraries(LeyoChatLauncher PRIVATE ole32 shell32)
enable_project_warnings(LeyoChatLauncher)
endif()

if(Qt6HttpServer_FOUND)
foreach(_leyo_service_target IN ITEMS LeyoChatService)
    target_include_directories(${_leyo_service_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fileservice
    )
    if(MSVC)
        target_compile_definitions(${_leyo_service_target} PRIVATE NOMINMAX)
    endif()
    target_link_libraries(${_leyo_service_target} PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::HttpServer Qt6::Network Qt6::Sql)
    enable_project_warnings(${_leyo_service_target})
endforeach()
set_target_properties(LeyoChatService PROPERTIES OUTPUT_NAME "LeyoChatService")

target_include_directories(LeyoChatServiceHost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_definitions(LeyoChatServiceHost PRIVATE NOMINMAX)
endif()
target_link_libraries(LeyoChatServiceHost PRIVATE Qt6::Core)
if(WIN32)
    target_link_libraries(LeyoChatServiceHost PRIVATE advapi32)
endif()
enable_project_warnings(LeyoChatServiceHost)
set_target_properties(LeyoChatServiceHost PROPERTIES OUTPUT_NAME "LeyoChatServiceHost")
endif()

# Statically link the MinGW runtime we control so the packaged app does not
# accidentally mix incompatible libstdc++/libwinpthread DLLs from the host.
if(MINGW)
    target_link_options(LeyoChat PRIVATE
        -static-libstdc++
        -static-libgcc
        -Wl,-Bstatic,-lwinpthread,-Bdynamic
    )
endif()

install(TARGETS LeyoChat
    RUNTIME DESTINATION .
)
if(WIN32)
install(TARGETS LeyoChatLauncher
    RUNTIME DESTINATION .
)
endif()
install(DIRECTORY resources/branding/
    DESTINATION resources/branding
)
install(FILES
    LICENSE
    NOTICE
    THIRD_PARTY_NOTICES.md
    DESTINATION licenses
)
install(FILES third_party/ElaWidgetTools/LICENSE
    DESTINATION licenses
    RENAME ElaWidgetTools-MIT.txt
)
install(FILES third_party/ElaWidgetTools/UPSTREAM.md
    DESTINATION licenses
    RENAME ElaWidgetTools-UPSTREAM.md
)
install(FILES third_party/md4c/LICENSE.md
    DESTINATION licenses
    RENAME md4c-MIT.txt
)
install(FILES windows/ChineseSimplified.LICENSE
    DESTINATION licenses
    RENAME InnoSetup-ChineseSimplified-MIT.txt
)
install(FILES
    LICENSES/LGPL-3.0-only.txt
    LICENSES/GPL-3.0-only.txt
    LICENSES/OFL-1.1.txt
    DESTINATION licenses
)

set(CPACK_PACKAGE_NAME "LeyoChat")
set(CPACK_PACKAGE_VENDOR "LeXiaoYao2704")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "LeyoChat")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "LeyoChat-${PROJECT_VERSION}-windows")
set(CPACK_NSIS_PACKAGE_NAME "LeyoChat")
set(CPACK_NSIS_DISPLAY_NAME "LeyoChat")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_MUI_ICON_CODE "")
set(CPACK_PACKAGE_EXECUTABLES "LeyoChat" "LeyoChat")

if(LEYOCHAT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ── 安装器 UI（Qt Quick） ──────────────────────────────────
option(LEYOCHAT_BUILD_INSTALLER "Build the LeyoChat graphical installer" OFF)
if(LEYOCHAT_BUILD_INSTALLER)
    find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
    if(Qt6Quick_FOUND AND Qt6QuickControls2_FOUND)
        add_subdirectory(installer)
    else()
        message(STATUS "Qt6::Quick not found — installer target skipped")
    endif()
endif()

include(CPack)
