find_package(Qt6 QUIET COMPONENTS HttpServer)
find_package(Python3 QUIET COMPONENTS Interpreter)

if(Python3_Interpreter_FOUND)
    add_test(NAME LeyoChatServicePressureToolTests
        COMMAND ${Python3_EXECUTABLE} -m unittest
                ${CMAKE_CURRENT_SOURCE_DIR}/TestLeyoChatServicePressureTool.py)
    set_tests_properties(LeyoChatServicePressureToolTests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

add_executable(LeyoChatTests
    TestMessageCodec.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/network/MessageCodec.cpp
    ../src/network/PeerHandshake.cpp
    ../src/services/ResourceRefRouter.cpp
)

target_include_directories(LeyoChatTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatTests)

add_test(NAME LeyoChatTests COMMAND LeyoChatTests)

add_executable(LeyoChatAppStyleThemeTokenTests
    TestAppStyleThemeTokens.cpp
)

target_include_directories(LeyoChatAppStyleThemeTokenTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAppStyleThemeTokenTests PRIVATE Qt6::Core Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatAppStyleThemeTokenTests)

add_test(NAME LeyoChatAppStyleThemeTokenTests COMMAND LeyoChatAppStyleThemeTokenTests)
set_tests_properties(LeyoChatAppStyleThemeTokenTests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>"
)

add_executable(LeyoChatCallSessionTests
    TestCallSession.cpp
    ../src/call/CallSession.cpp
)

target_include_directories(LeyoChatCallSessionTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatCallSessionTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatCallSessionTests)

add_test(NAME LeyoChatCallSessionTests COMMAND LeyoChatCallSessionTests)
set_tests_properties(LeyoChatCallSessionTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatMarkdownRendererTests
    TestMarkdownRenderer.cpp
    ../src/ui/MarkdownRenderer.cpp
)

target_include_directories(LeyoChatMarkdownRendererTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMarkdownRendererTests PRIVATE Qt6::Core Qt6::Test md4c)
enable_project_warnings(LeyoChatMarkdownRendererTests)

add_test(NAME LeyoChatMarkdownRendererTests COMMAND LeyoChatMarkdownRendererTests)
set_tests_properties(LeyoChatMarkdownRendererTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:md4c>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatResourceRefRouterTests
    TestResourceRefRouter.cpp
    ../src/services/ResourceRefRouter.cpp
)

target_include_directories(LeyoChatResourceRefRouterTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatResourceRefRouterTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatResourceRefRouterTests)

add_test(NAME LeyoChatResourceRefRouterTests COMMAND LeyoChatResourceRefRouterTests)

add_executable(LeyoChatDevOpsAdapterContractsTests
    TestDevOpsAdapterContracts.cpp
    ../src/integrations/DevOpsAdapterContracts.cpp
)

target_include_directories(LeyoChatDevOpsAdapterContractsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatDevOpsAdapterContractsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatDevOpsAdapterContractsTests)

add_test(NAME LeyoChatDevOpsAdapterContractsTests COMMAND LeyoChatDevOpsAdapterContractsTests)

add_executable(TestConversationListModel
    TestConversationListModel.cpp
    ../src/ui/ConversationListModel.cpp
    ../src/store/ChatDataStore.cpp
    ../src/ui/PinyinHelper.cpp
)
target_include_directories(TestConversationListModel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(TestConversationListModel PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
    ElaWidgetTools
)
enable_project_warnings(TestConversationListModel)
add_test(NAME TestConversationListModel COMMAND TestConversationListModel)
set_tests_properties(TestConversationListModel PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatAzureDevOpsSettingsTests
    TestAzureDevOpsSettings.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
)

target_include_directories(LeyoChatAzureDevOpsSettingsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsSettingsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsSettingsTests)

add_test(NAME LeyoChatAzureDevOpsSettingsTests COMMAND LeyoChatAzureDevOpsSettingsTests)

add_executable(LeyoChatAzureDevOpsLinkParserTests
    TestAzureDevOpsLinkParser.cpp
    ../src/integrations/AzureDevOpsLinkParser.cpp
)

target_include_directories(LeyoChatAzureDevOpsLinkParserTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsLinkParserTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsLinkParserTests)

add_test(NAME LeyoChatAzureDevOpsLinkParserTests COMMAND LeyoChatAzureDevOpsLinkParserTests)

add_executable(LeyoChatLocalAzureDevOpsAdapterTests
    TestLocalAzureDevOpsAdapter.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/AzureDevOpsLinkParser.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
    ../src/integrations/DevOpsAdapterContracts.cpp
    ../src/integrations/LocalAzureDevOpsAdapter.cpp
)

target_include_directories(LeyoChatLocalAzureDevOpsAdapterTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatLocalAzureDevOpsAdapterTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatLocalAzureDevOpsAdapterTests)

add_test(NAME LeyoChatLocalAzureDevOpsAdapterTests COMMAND LeyoChatLocalAzureDevOpsAdapterTests)

add_executable(LeyoChatAzureDevOpsNotificationContractsTests
    TestAzureDevOpsNotificationContracts.cpp
    ../src/integrations/AzureDevOpsNotificationContracts.cpp
)

target_include_directories(LeyoChatAzureDevOpsNotificationContractsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsNotificationContractsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsNotificationContractsTests)

add_test(NAME LeyoChatAzureDevOpsNotificationContractsTests COMMAND LeyoChatAzureDevOpsNotificationContractsTests)

add_executable(LeyoChatAzureDevOpsInsertDialogTests
    TestAzureDevOpsInsertDialog.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/AzureDevOpsInsertDialog.cpp
    ../src/integrations/AzureDevOpsLinkParser.cpp
)

target_include_directories(LeyoChatAzureDevOpsInsertDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsInsertDialogTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatAzureDevOpsInsertDialogTests)

add_test(NAME LeyoChatAzureDevOpsInsertDialogTests COMMAND LeyoChatAzureDevOpsInsertDialogTests)

add_executable(LeyoChatAzureDevOpsNotificationDialogTests
    TestAzureDevOpsNotificationDialog.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/AzureDevOpsNotificationDialog.cpp
    ../src/integrations/AzureDevOpsNotificationContracts.cpp
)

target_include_directories(LeyoChatAzureDevOpsNotificationDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsNotificationDialogTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatAzureDevOpsNotificationDialogTests)

add_test(NAME LeyoChatAzureDevOpsNotificationDialogTests COMMAND LeyoChatAzureDevOpsNotificationDialogTests)

add_executable(LeyoChatAzureDevOpsNotificationDispatcherTests
    TestAzureDevOpsNotificationDispatcher.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/integrations/AzureDevOpsNotificationContracts.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/AzureDevOpsNotificationDispatcher.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/GroupService.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatAzureDevOpsNotificationDispatcherTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsNotificationDispatcherTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsNotificationDispatcherTests)

add_test(NAME LeyoChatAzureDevOpsNotificationDispatcherTests COMMAND LeyoChatAzureDevOpsNotificationDispatcherTests)

add_executable(LeyoChatAzureDevOpsBuildNotificationPollerTests
    TestAzureDevOpsBuildNotificationPoller.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
    ../src/integrations/AzureDevOpsLinkParser.cpp
    ../src/integrations/DevOpsAdapterContracts.cpp
    ../src/integrations/LocalAzureDevOpsAdapter.cpp
    ../src/services/AzureDevOpsBuildNotificationPoller.cpp
)

target_include_directories(LeyoChatAzureDevOpsBuildNotificationPollerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsBuildNotificationPollerTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsBuildNotificationPollerTests)

add_test(NAME LeyoChatAzureDevOpsBuildNotificationPollerTests COMMAND LeyoChatAzureDevOpsBuildNotificationPollerTests)

add_executable(LeyoChatOutlookAdapterContractsTests
    TestOutlookAdapterContracts.cpp
    ../src/integrations/OutlookAdapterContracts.cpp
)

target_include_directories(LeyoChatOutlookAdapterContractsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookAdapterContractsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatOutlookAdapterContractsTests)

add_test(NAME LeyoChatOutlookAdapterContractsTests COMMAND LeyoChatOutlookAdapterContractsTests)

add_executable(LeyoChatOutlookSettingsTests
    TestOutlookSettings.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/OutlookSettings.cpp
)

target_include_directories(LeyoChatOutlookSettingsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookSettingsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatOutlookSettingsTests)

add_test(NAME LeyoChatOutlookSettingsTests COMMAND LeyoChatOutlookSettingsTests)

add_executable(LeyoChatOutlookEwsTransportTests
    TestOutlookEwsTransport.cpp
    ../src/integrations/OutlookEwsTransport.cpp
    ../src/integrations/OutlookSettings.cpp
    ../src/integrations/OutlookAdapterContracts.cpp
    ../src/app/AppSettings.cpp
)
target_include_directories(LeyoChatOutlookEwsTransportTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookEwsTransportTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatOutlookEwsTransportTests)
add_test(NAME LeyoChatOutlookEwsTransportTests COMMAND LeyoChatOutlookEwsTransportTests)

# OutlookOAuthManager removed 鈥?replaced by OutlookEwsTransport (see LeyoChatOutlookEwsTransportTests)

add_executable(LeyoChatOutlookNotificationContractsTests
    TestOutlookNotificationContracts.cpp
    ../src/integrations/OutlookNotificationContracts.cpp
)

target_include_directories(LeyoChatOutlookNotificationContractsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookNotificationContractsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatOutlookNotificationContractsTests)

add_test(NAME LeyoChatOutlookNotificationContractsTests COMMAND LeyoChatOutlookNotificationContractsTests)

add_executable(LeyoChatOutlookNotificationDialogTests
    TestOutlookNotificationDialog.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/OutlookSettings.cpp
    ../src/ui/OutlookNotificationDialog.cpp
)

target_include_directories(LeyoChatOutlookNotificationDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookNotificationDialogTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatOutlookNotificationDialogTests)

add_test(NAME LeyoChatOutlookNotificationDialogTests COMMAND LeyoChatOutlookNotificationDialogTests)

add_executable(LeyoChatOutlookNotificationDispatcherTests
    TestOutlookNotificationDispatcher.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/integrations/OutlookNotificationContracts.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/GroupService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/OutlookNotificationDispatcher.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatOutlookNotificationDispatcherTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookNotificationDispatcherTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatOutlookNotificationDispatcherTests)

add_test(NAME LeyoChatOutlookNotificationDispatcherTests COMMAND LeyoChatOutlookNotificationDispatcherTests)

add_executable(LeyoChatOutlookNotificationPollerTests
    TestOutlookNotificationPoller.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/OutlookAdapterContracts.cpp
    ../src/integrations/OutlookNotificationContracts.cpp
    ../src/integrations/OutlookSettings.cpp
    ../src/integrations/OutlookEwsTransport.cpp
    ../src/integrations/LocalOutlookAdapter.cpp
    ../src/services/OutlookNotificationPoller.cpp
)

target_include_directories(LeyoChatOutlookNotificationPollerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookNotificationPollerTests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatOutlookNotificationPollerTests)

add_test(NAME LeyoChatOutlookNotificationPollerTests COMMAND LeyoChatOutlookNotificationPollerTests)

add_executable(LeyoChatAzureDevOpsIntegrationHealthTests
    TestAzureDevOpsIntegrationHealth.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
    ../src/integrations/AzureDevOpsLinkParser.cpp
    ../src/integrations/AzureDevOpsNotificationContracts.cpp
    ../src/integrations/DevOpsAdapterContracts.cpp
    ../src/integrations/LocalAzureDevOpsAdapter.cpp
    ../src/services/AzureDevOpsBuildNotificationPoller.cpp
)

target_include_directories(LeyoChatAzureDevOpsIntegrationHealthTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAzureDevOpsIntegrationHealthTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatAzureDevOpsIntegrationHealthTests)

add_test(NAME LeyoChatAzureDevOpsIntegrationHealthTests COMMAND LeyoChatAzureDevOpsIntegrationHealthTests)
set_tests_properties(LeyoChatAzureDevOpsIntegrationHealthTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatOutlookIntegrationHealthTests
    TestOutlookIntegrationHealth.cpp
    ../src/app/AppSettings.cpp
    ../src/integrations/OutlookAdapterContracts.cpp
    ../src/integrations/OutlookEwsTransport.cpp
    ../src/integrations/OutlookNotificationContracts.cpp
    ../src/integrations/OutlookSettings.cpp
    ../src/integrations/LocalOutlookAdapter.cpp
    ../src/services/OutlookNotificationPoller.cpp
    ../src/services/OutlookStreamingConnection.cpp
)

target_include_directories(LeyoChatOutlookIntegrationHealthTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutlookIntegrationHealthTests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatOutlookIntegrationHealthTests)

add_test(NAME LeyoChatOutlookIntegrationHealthTests COMMAND LeyoChatOutlookIntegrationHealthTests)

add_executable(LeyoChatSharedFileResourceContractsTests
    TestSharedFileResourceContracts.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
)

target_include_directories(LeyoChatSharedFileResourceContractsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatSharedFileResourceContractsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatSharedFileResourceContractsTests)

add_test(NAME LeyoChatSharedFileResourceContractsTests COMMAND LeyoChatSharedFileResourceContractsTests)

add_executable(LeyoChatApplicationInfoTests
    TestApplicationInfo.cpp
    ../src/app/ApplicationInfo.cpp
    ../windows/leyochat.qrc
)

target_include_directories(LeyoChatApplicationInfoTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}/../generated
)
target_link_libraries(LeyoChatApplicationInfoTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatApplicationInfoTests)

add_test(NAME LeyoChatApplicationInfoTests COMMAND LeyoChatApplicationInfoTests)

add_executable(LeyoChatClientStartupOptionsTests
    TestClientStartupOptions.cpp
    ../src/recovery/ClientStartupOptions.cpp
)

target_include_directories(LeyoChatClientStartupOptionsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatClientStartupOptionsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatClientStartupOptionsTests)

add_test(NAME LeyoChatClientStartupOptionsTests COMMAND LeyoChatClientStartupOptionsTests)
set_tests_properties(LeyoChatClientStartupOptionsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatClientRecoveryStateTests
    TestClientRecoveryState.cpp
    ../src/recovery/ClientRecoveryState.cpp
)

target_include_directories(LeyoChatClientRecoveryStateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatClientRecoveryStateTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatClientRecoveryStateTests)

add_test(NAME LeyoChatClientRecoveryStateTests COMMAND LeyoChatClientRecoveryStateTests)
set_tests_properties(LeyoChatClientRecoveryStateTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatLauncherPolicyTests
    TestLauncherPolicy.cpp
    ../src/launcher/LauncherPolicy.cpp
)

target_include_directories(LeyoChatLauncherPolicyTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatLauncherPolicyTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatLauncherPolicyTests)

add_test(NAME LeyoChatLauncherPolicyTests COMMAND LeyoChatLauncherPolicyTests)
set_tests_properties(LeyoChatLauncherPolicyTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatInstallerLaunchPolicyTests
    TestInstallerLaunchPolicy.cpp
)

target_include_directories(LeyoChatInstallerLaunchPolicyTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(LeyoChatInstallerLaunchPolicyTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatInstallerLaunchPolicyTests)

add_test(NAME LeyoChatInstallerLaunchPolicyTests COMMAND LeyoChatInstallerLaunchPolicyTests)

add_executable(LeyoChatIntegrationNotificationPresentationTests
    TestIntegrationNotificationPresentation.cpp
    ../src/app/IntegrationNotificationPresentation.cpp
)

target_include_directories(LeyoChatIntegrationNotificationPresentationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatIntegrationNotificationPresentationTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatIntegrationNotificationPresentationTests)

add_test(NAME LeyoChatIntegrationNotificationPresentationTests COMMAND LeyoChatIntegrationNotificationPresentationTests)

add_executable(LeyoChatIntegrationPollBackoffTests
    TestIntegrationPollBackoff.cpp
    ../src/app/IntegrationPollBackoff.cpp
)

target_include_directories(LeyoChatIntegrationPollBackoffTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatIntegrationPollBackoffTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatIntegrationPollBackoffTests)

add_test(NAME LeyoChatIntegrationPollBackoffTests COMMAND LeyoChatIntegrationPollBackoffTests)

add_executable(LeyoChatReminderTimeOptionsTests
    TestReminderTimeOptions.cpp
    ../src/services/ReminderTimeOptions.cpp
)

target_include_directories(LeyoChatReminderTimeOptionsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderTimeOptionsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReminderTimeOptionsTests)

add_test(NAME LeyoChatReminderTimeOptionsTests COMMAND LeyoChatReminderTimeOptionsTests)
set_tests_properties(LeyoChatReminderTimeOptionsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatReminderItemFactoryTests
    TestReminderItemFactory.cpp
    ../src/app/ReminderItemFactory.cpp
    ../src/services/ReminderTimeOptions.cpp
)

target_include_directories(LeyoChatReminderItemFactoryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderItemFactoryTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReminderItemFactoryTests)

add_test(NAME LeyoChatReminderItemFactoryTests COMMAND LeyoChatReminderItemFactoryTests)
set_tests_properties(LeyoChatReminderItemFactoryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatTransferSpeedTrackerTests
    TestTransferSpeedTracker.cpp
    ../src/app/TransferSpeedTracker.cpp
)

target_include_directories(LeyoChatTransferSpeedTrackerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatTransferSpeedTrackerTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatTransferSpeedTrackerTests)

add_test(NAME LeyoChatTransferSpeedTrackerTests COMMAND LeyoChatTransferSpeedTrackerTests)

add_executable(LeyoChatHttpFileClientSourceTests
    TestHttpFileClientSource.cpp
)

target_compile_definitions(LeyoChatHttpFileClientSourceTests PRIVATE
    LEYOCHAT_HTTP_FILE_CLIENT_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/network/HttpFileClient.cpp")
target_link_libraries(LeyoChatHttpFileClientSourceTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatHttpFileClientSourceTests)

add_test(NAME LeyoChatHttpFileClientSourceTests COMMAND LeyoChatHttpFileClientSourceTests)
set_tests_properties(LeyoChatHttpFileClientSourceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatFerryBrowserWidgetSourceTests
    TestFerryBrowserWidgetSource.cpp
)

target_compile_definitions(LeyoChatFerryBrowserWidgetSourceTests PRIVATE
    LEYOCHAT_FERRY_BROWSER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/FerryBrowserWidget.cpp")
target_link_libraries(LeyoChatFerryBrowserWidgetSourceTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatFerryBrowserWidgetSourceTests)

add_test(NAME LeyoChatFerryBrowserWidgetSourceTests COMMAND LeyoChatFerryBrowserWidgetSourceTests)
set_tests_properties(LeyoChatFerryBrowserWidgetSourceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatUiRestoreHelpersTests
    TestUiRestoreHelpers.cpp
    ../src/app/UiRestoreHelpers.cpp
)

target_include_directories(LeyoChatUiRestoreHelpersTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatUiRestoreHelpersTests PRIVATE Qt6::Core Qt6::Widgets Qt6::Test)
enable_project_warnings(LeyoChatUiRestoreHelpersTests)

add_test(NAME LeyoChatUiRestoreHelpersTests COMMAND LeyoChatUiRestoreHelpersTests)

add_executable(LeyoChatImageFileTypeHelpersTests
    TestImageFileTypeHelpers.cpp
    ../src/app/ImageFileTypeHelpers.cpp
)

target_include_directories(LeyoChatImageFileTypeHelpersTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatImageFileTypeHelpersTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatImageFileTypeHelpersTests)

add_test(NAME LeyoChatImageFileTypeHelpersTests COMMAND LeyoChatImageFileTypeHelpersTests)

add_executable(LeyoChatMessagePresentationHelpersTests
    TestMessagePresentationHelpers.cpp
    ../src/app/MessagePresentationHelpers.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/services/ResourceRefRouter.cpp
)

target_include_directories(LeyoChatMessagePresentationHelpersTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(LeyoChatMessagePresentationHelpersTests PRIVATE
    LEYOCHAT_MESSAGE_BUBBLE_WIDGET_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MessageBubbleWidget.cpp")
target_link_libraries(LeyoChatMessagePresentationHelpersTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
enable_project_warnings(LeyoChatMessagePresentationHelpersTests)

add_test(NAME LeyoChatMessagePresentationHelpersTests COMMAND LeyoChatMessagePresentationHelpersTests)

add_executable(LeyoChatMessageTextLinkifierTests
    TestMessageTextLinkifier.cpp
    ../src/ui/MessageTextLinkifier.cpp
)

target_include_directories(LeyoChatMessageTextLinkifierTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageTextLinkifierTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
enable_project_warnings(LeyoChatMessageTextLinkifierTests)

add_test(NAME LeyoChatMessageTextLinkifierTests COMMAND LeyoChatMessageTextLinkifierTests)
set_tests_properties(LeyoChatMessageTextLinkifierTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatAttachmentPayloadHelpersTests
    TestAttachmentPayloadHelpers.cpp
    ../src/app/AppSettings.cpp
    ../src/app/AppPathHelpers.cpp
    ../src/app/AttachmentPayloadHelpers.cpp
    ../src/app/TestModeContext.cpp
)

target_include_directories(LeyoChatAttachmentPayloadHelpersTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAttachmentPayloadHelpersTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatAttachmentPayloadHelpersTests)

add_test(NAME LeyoChatAttachmentPayloadHelpersTests COMMAND LeyoChatAttachmentPayloadHelpersTests)

add_executable(LeyoChatPeerPresentationHelpersTests
    TestPeerPresentationHelpers.cpp
    ../src/app/PeerPresentationHelpers.cpp
)

target_include_directories(LeyoChatPeerPresentationHelpersTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatPeerPresentationHelpersTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatPeerPresentationHelpersTests)

add_test(NAME LeyoChatPeerPresentationHelpersTests COMMAND LeyoChatPeerPresentationHelpersTests)

add_executable(LeyoChatDevelopmentTestModeTests
    TestDevelopmentTestMode.cpp
    ../src/app/AppSettings.cpp
    ../src/app/TestModeContext.cpp
    ../src/diagnostics/Diagnostics.cpp
    ../src/integrations/OutlookSettings.cpp
)

target_include_directories(LeyoChatDevelopmentTestModeTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(LeyoChatDevelopmentTestModeTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatDevelopmentTestModeTests)

add_test(NAME LeyoChatDevelopmentTestModeTests COMMAND LeyoChatDevelopmentTestModeTests)

add_executable(LeyoChatContactListModelTests
    TestContactListModel.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/ContactListModel.cpp
    ../src/store/ChatDataStore.cpp
    ../src/ui/PinyinHelper.cpp
)

target_include_directories(LeyoChatContactListModelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatContactListModelTests PRIVATE Qt6::Core Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatContactListModelTests)

add_test(NAME LeyoChatContactListModelTests COMMAND LeyoChatContactListModelTests)

add_executable(LeyoChatPeerDirectoryServiceTests
    TestPeerDirectoryService.cpp
    ../src/services/PeerDirectoryService.cpp
)

target_include_directories(LeyoChatPeerDirectoryServiceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatPeerDirectoryServiceTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatPeerDirectoryServiceTests)

add_test(NAME LeyoChatPeerDirectoryServiceTests COMMAND LeyoChatPeerDirectoryServiceTests)

add_executable(LeyoChatConnectionRegistryUtilsTests
    TestConnectionRegistryUtils.cpp
)

target_include_directories(LeyoChatConnectionRegistryUtilsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(LeyoChatConnectionRegistryUtilsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/app)
target_link_libraries(LeyoChatConnectionRegistryUtilsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatConnectionRegistryUtilsTests)

add_test(NAME LeyoChatConnectionRegistryUtilsTests COMMAND LeyoChatConnectionRegistryUtilsTests)
set_tests_properties(LeyoChatConnectionRegistryUtilsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatMessageListModelTests
    TestMessageListModel.cpp
    ../src/ui/MessageListModel.cpp
    ../src/ui/MessageDeliveryPresentation.cpp
    ../src/store/ChatDataStore.cpp
)

target_include_directories(LeyoChatMessageListModelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageListModelTests PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Widgets
    Qt6::Test
    ElaWidgetTools
)
enable_project_warnings(LeyoChatMessageListModelTests)

add_test(NAME LeyoChatMessageListModelTests COMMAND LeyoChatMessageListModelTests)

add_executable(LeyoChatMessageDeliveryPresentationTests
    TestMessageDeliveryPresentation.cpp
    ../src/ui/MessageDeliveryPresentation.cpp
)

target_include_directories(LeyoChatMessageDeliveryPresentationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageDeliveryPresentationTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatMessageDeliveryPresentationTests)

add_test(NAME LeyoChatMessageDeliveryPresentationTests
    COMMAND LeyoChatMessageDeliveryPresentationTests)
set_tests_properties(LeyoChatMessageDeliveryPresentationTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatPresenceHeuristicsTests
    TestPresenceHeuristics.cpp
)

target_include_directories(LeyoChatPresenceHeuristicsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatPresenceHeuristicsTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatPresenceHeuristicsTests)

add_test(NAME LeyoChatPresenceHeuristicsTests COMMAND LeyoChatPresenceHeuristicsTests)

add_executable(LeyoChatRemotePresenceUiAdapterTests
    TestRemotePresenceUiAdapter.cpp
    ../src/app/RemotePresenceUiAdapter.cpp
    ../src/services/DirectConversationAddressing.cpp
)

target_include_directories(LeyoChatRemotePresenceUiAdapterTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemotePresenceUiAdapterTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatRemotePresenceUiAdapterTests)

add_test(NAME LeyoChatRemotePresenceUiAdapterTests COMMAND LeyoChatRemotePresenceUiAdapterTests)
set_tests_properties(LeyoChatRemotePresenceUiAdapterTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

add_executable(LeyoChatChatServiceTests
    TestChatService.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/services/ChatService.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/ui/StickerManager.cpp
)

target_include_directories(LeyoChatChatServiceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatChatServiceTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test Qt6::Widgets)
enable_project_warnings(LeyoChatChatServiceTests)

add_test(NAME LeyoChatChatServiceTests COMMAND LeyoChatChatServiceTests)

add_executable(LeyoChatIdentityTests
    TestIdentityService.cpp
    ../src/services/IdentityService.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ProfileRepository.cpp
)

target_include_directories(LeyoChatIdentityTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatIdentityTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatIdentityTests)

add_test(NAME LeyoChatIdentityTests COMMAND LeyoChatIdentityTests)

add_executable(LeyoChatDatabaseTests
    TestDatabaseManager.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/GroupRepository.cpp
    ../src/storage/ProfileRepository.cpp
    ../src/storage/ServiceBindingRepository.cpp
    ../src/storage/ServiceRegistryRepository.cpp
    ../src/storage/ServiceResourceRepository.cpp
)

target_include_directories(LeyoChatDatabaseTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatDatabaseTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatDatabaseTests)

add_test(NAME LeyoChatDatabaseTests COMMAND LeyoChatDatabaseTests)

add_executable(LeyoChatGroupRepositoryTests
    TestGroupRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatGroupRepositoryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupRepositoryTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatGroupRepositoryTests)

add_test(NAME LeyoChatGroupRepositoryTests COMMAND LeyoChatGroupRepositoryTests)

add_executable(LeyoChatGroupServiceTests
    TestGroupService.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/GroupService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatGroupServiceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupServiceTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatGroupServiceTests)

add_test(NAME LeyoChatGroupServiceTests COMMAND LeyoChatGroupServiceTests)

add_executable(LeyoChatGroupFileSendRoutingTests
    TestGroupFileSendRouting.cpp
    ../src/app/GroupFileServiceConfigResolver.cpp
    ../src/app/GroupFileSendRouting.cpp
    ../src/integrations/RemoteChatServiceSettings.cpp
    ../src/app/AppSettings.cpp
)

target_include_directories(LeyoChatGroupFileSendRoutingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupFileSendRoutingTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatGroupFileSendRoutingTests)

add_test(NAME LeyoChatGroupFileSendRoutingTests COMMAND LeyoChatGroupFileSendRoutingTests)

add_executable(LeyoChatGroupResourceRefFanOutTests
    TestGroupResourceRefFanOut.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/GroupService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatGroupResourceRefFanOutTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupResourceRefFanOutTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatGroupResourceRefFanOutTests)

add_test(NAME LeyoChatGroupResourceRefFanOutTests COMMAND LeyoChatGroupResourceRefFanOutTests)

add_executable(LeyoChatFileTransferTests
    TestFileTransferRepository.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/DatabaseManager.cpp
)

target_include_directories(LeyoChatFileTransferTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatFileTransferTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatFileTransferTests)

add_test(NAME LeyoChatFileTransferTests COMMAND LeyoChatFileTransferTests)

add_executable(LeyoChatReminderRepositoryTests
    TestReminderRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ReminderRepository.cpp
)

target_include_directories(LeyoChatReminderRepositoryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderRepositoryTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatReminderRepositoryTests)

add_test(NAME LeyoChatReminderRepositoryTests COMMAND LeyoChatReminderRepositoryTests)

add_executable(LeyoChatReminderServiceTests
    TestReminderService.cpp
    ../src/services/ReminderService.cpp
    ../src/services/ReminderTimeOptions.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ReminderRepository.cpp
)

target_include_directories(LeyoChatReminderServiceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderServiceTests PRIVATE Qt6::Core Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatReminderServiceTests)

add_test(NAME LeyoChatReminderServiceTests COMMAND LeyoChatReminderServiceTests)

add_executable(LeyoChatReminderDialogTests
    TestReminderDialog.cpp
    ../src/app/AppSettings.cpp
    ../src/services/ReminderTimeOptions.cpp
    ../src/ui/ReminderDialog.cpp
)

target_include_directories(LeyoChatReminderDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderDialogTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatReminderDialogTests)

add_test(NAME LeyoChatReminderDialogTests COMMAND LeyoChatReminderDialogTests)

add_executable(LeyoChatReminderSignalContractTests
    TestReminderSignalContracts.cpp
    ../src/ui/ConversationsPage.h
    ../src/app/AppSettings.cpp
    ../src/ui/GroupFileManagerDialog.cpp
    ../src/ui/GroupFileTableModel.cpp
    ../src/ui/GroupFileVersionDialog.cpp
    ../src/ui/FilePreviewWidget.cpp
    ../src/ui/MarkdownRenderer.cpp
)

target_include_directories(LeyoChatReminderSignalContractTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(LeyoChatReminderSignalContractTests PRIVATE Qt6::Widgets Qt6::Network Qt6::Test md4c ElaWidgetTools)
enable_project_warnings(LeyoChatReminderSignalContractTests)

add_test(NAME LeyoChatReminderSignalContractTests COMMAND LeyoChatReminderSignalContractTests)

add_executable(LeyoChatReminderNotificationPresentationTests
    TestReminderNotificationPresentation.cpp
)

target_include_directories(LeyoChatReminderNotificationPresentationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderNotificationPresentationTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReminderNotificationPresentationTests)

add_test(NAME LeyoChatReminderNotificationPresentationTests COMMAND LeyoChatReminderNotificationPresentationTests)

add_executable(LeyoChatReminderActionRoutingTests
    TestReminderActionRouting.cpp
)

target_include_directories(LeyoChatReminderActionRoutingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderActionRoutingTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReminderActionRoutingTests)

add_test(NAME LeyoChatReminderActionRoutingTests COMMAND LeyoChatReminderActionRoutingTests)

add_executable(LeyoChatReminderListModelTests
    TestReminderListModel.cpp
    ../src/ui/ReminderListModel.cpp
)

target_include_directories(LeyoChatReminderListModelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderListModelTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReminderListModelTests)

add_test(NAME LeyoChatReminderListModelTests COMMAND LeyoChatReminderListModelTests)

add_executable(LeyoChatReminderManagementSurfaceContractTests
    TestReminderManagementSurfaceContracts.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/NotificationsPage.cpp
)

target_include_directories(LeyoChatReminderManagementSurfaceContractTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReminderManagementSurfaceContractTests PRIVATE Qt6::Core Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatReminderManagementSurfaceContractTests)

add_test(NAME LeyoChatReminderManagementSurfaceContractTests COMMAND LeyoChatReminderManagementSurfaceContractTests)

add_executable(LeyoChatFileTransferServiceSourceTests
    TestFileTransferServiceSource.cpp
)

target_compile_definitions(LeyoChatFileTransferServiceSourceTests PRIVATE
    LEYOCHAT_FILE_TRANSFER_SERVICE_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/services/FileTransferService.cpp")
target_link_libraries(LeyoChatFileTransferServiceSourceTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatFileTransferServiceSourceTests)

add_test(NAME LeyoChatFileTransferServiceSourceTests
    COMMAND LeyoChatFileTransferServiceSourceTests)

add_executable(LeyoChatFileTransferServiceTests
    TestFileTransferService.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/DatabaseManager.cpp
)

target_include_directories(LeyoChatFileTransferServiceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatFileTransferServiceTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileTransferServiceTests)

add_test(NAME LeyoChatFileTransferServiceTests COMMAND LeyoChatFileTransferServiceTests)

add_executable(LeyoChatOutgoingFileSnapshotTests
    TestOutgoingFileSnapshot.cpp
    ../src/services/OutgoingFileSnapshot.cpp
)

target_include_directories(LeyoChatOutgoingFileSnapshotTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatOutgoingFileSnapshotTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatOutgoingFileSnapshotTests)

add_test(NAME LeyoChatOutgoingFileSnapshotTests COMMAND LeyoChatOutgoingFileSnapshotTests)
set_tests_properties(LeyoChatOutgoingFileSnapshotTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatLanDiscoveryTests
    TestLanDiscovery.cpp
    ../src/network/LanDiscoveryService.cpp
)

target_include_directories(LeyoChatLanDiscoveryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatLanDiscoveryTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatLanDiscoveryTests)

add_test(NAME LeyoChatLanDiscoveryTests COMMAND LeyoChatLanDiscoveryTests)

add_executable(LeyoChatNetworkTests
    TestPeerConnection.cpp
    ../src/network/PeerConnection.cpp
    ../src/network/TlsHelper.cpp
)

target_include_directories(LeyoChatNetworkTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatNetworkTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatNetworkTests)

add_test(NAME LeyoChatNetworkTests COMMAND LeyoChatNetworkTests)

add_executable(LeyoChatTlsTests
    TestPeerConnectionTls.cpp
    ../src/network/PeerConnection.cpp
    ../src/network/PeerHandshake.cpp
    ../src/network/TlsHelper.cpp
    ../windows/leyochat.qrc
)

target_include_directories(LeyoChatTlsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatTlsTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatTlsTests)

add_test(NAME LeyoChatTlsTests COMMAND LeyoChatTlsTests)
set_tests_properties(LeyoChatTlsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatDelegateTests
    TestDelegates.cpp
    ../src/app/AppSettings.cpp
    ../src/services/ResourceRefRouter.cpp
)

target_include_directories(LeyoChatDelegateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatDelegateTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatDelegateTests)

add_test(NAME LeyoChatDelegateTests COMMAND LeyoChatDelegateTests)

add_executable(LeyoChatChatHeaderWidgetTests
    TestChatHeaderWidget.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/ChatHeaderWidget.cpp
)

target_include_directories(LeyoChatChatHeaderWidgetTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatChatHeaderWidgetTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatChatHeaderWidgetTests)

add_test(NAME LeyoChatChatHeaderWidgetTests COMMAND LeyoChatChatHeaderWidgetTests)

add_executable(LeyoChatChatComposerWidgetTests
    TestChatComposerWidget.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/ChatComposerWidget.cpp
    ../src/ui/StickerManager.cpp
)

target_include_directories(LeyoChatChatComposerWidgetTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(LeyoChatChatComposerWidgetTests PRIVATE
    LEYOCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(LeyoChatChatComposerWidgetTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatChatComposerWidgetTests)

add_test(NAME LeyoChatChatComposerWidgetTests COMMAND LeyoChatChatComposerWidgetTests)

add_executable(LeyoChatMessageBubbleWidgetTests
    TestMessageBubbleWidget.cpp
    ../src/app/ImageFileTypeHelpers.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/MessageBubbleWidget.cpp
    ../src/ui/MessageBubbleDelegate.cpp
    ../src/ui/MessageDeliveryPresentation.cpp
    ../src/ui/MessageTextLinkifier.cpp
    ../src/ui/MessageThumbnailCache.cpp
    ../src/ui/StickerManager.cpp
    ../src/ui/FilePreviewWidget.cpp
    ../src/ui/MarkdownRenderer.cpp
    ../src/services/ResourceRefRouter.cpp
)

target_include_directories(LeyoChatMessageBubbleWidgetTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageBubbleWidgetTests PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
    md4c
    ElaWidgetTools
)
enable_project_warnings(LeyoChatMessageBubbleWidgetTests)

add_test(NAME LeyoChatMessageBubbleWidgetTests COMMAND LeyoChatMessageBubbleWidgetTests)

add_executable(LeyoChatTransferListModelTests
    TestTransferListModel.cpp
    ../src/ui/TransferListModel.cpp
)

target_include_directories(LeyoChatTransferListModelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatTransferListModelTests PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatTransferListModelTests)

add_test(NAME LeyoChatTransferListModelTests COMMAND LeyoChatTransferListModelTests)

add_executable(LeyoChatArchitectureReadinessTests
    TestArchitectureReadiness.cpp
    ../src/architecture/ArchitectureSnapshotAssembler.cpp
    ../src/architecture/HybridRoutingPolicy.cpp
    ../src/architecture/DatabaseResourceCatalog.cpp
    ../src/architecture/DatabaseServiceBindingCatalog.cpp
    ../src/architecture/DatabaseServiceDiscoveryProvider.cpp
    ../src/architecture/DatabaseServiceSelectionCatalog.cpp
    ../src/architecture/InMemoryResourceCatalog.cpp
    ../src/architecture/InMemoryServiceBindingCatalog.cpp
    ../src/architecture/InMemoryServiceSelectionCatalog.cpp
    ../src/architecture/PersistedRuntimeArchitectureLoader.cpp
    ../src/architecture/RuntimeArchitecturePresentation.cpp
    ../src/architecture/RuntimeArchitectureFacade.cpp
    ../src/architecture/RuntimeArchitectureQueryService.cpp
    ../src/architecture/StaticServiceDiscoveryProvider.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ServiceBindingRepository.cpp
    ../src/storage/ServiceRegistryRepository.cpp
    ../src/storage/ServiceResourceRepository.cpp
)

target_include_directories(LeyoChatArchitectureReadinessTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatArchitectureReadinessTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatArchitectureReadinessTests)

add_test(NAME LeyoChatArchitectureReadinessTests COMMAND LeyoChatArchitectureReadinessTests)

add_executable(LeyoChatDiagnosticsTests
    TestDiagnostics.cpp
    ../src/app/AppSettings.cpp
    ../src/app/TestModeContext.cpp
    ../src/diagnostics/Diagnostics.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
    ../src/integrations/OutlookSettings.cpp
)

target_include_directories(LeyoChatDiagnosticsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatDiagnosticsTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatDiagnosticsTests)

add_test(NAME LeyoChatDiagnosticsTests COMMAND LeyoChatDiagnosticsTests)

add_executable(LeyoChatGroupInfoPanelTests
    TestGroupInfoPanel.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/GroupInfoPanel.cpp
    ../src/ui/CreateGroupDialog.cpp
)

target_include_directories(LeyoChatGroupInfoPanelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupInfoPanelTests PRIVATE Qt6::Widgets Qt6::Network Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatGroupInfoPanelTests)

add_test(NAME LeyoChatGroupInfoPanelTests COMMAND LeyoChatGroupInfoPanelTests)

add_executable(LeyoChatRuntimeArchitectureDialogTests
    TestRuntimeArchitectureDialog.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/RuntimeArchitectureDialog.cpp
    ../src/architecture/RuntimeArchitecturePresentation.cpp
)

target_include_directories(LeyoChatRuntimeArchitectureDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRuntimeArchitectureDialogTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatRuntimeArchitectureDialogTests)

add_test(NAME LeyoChatRuntimeArchitectureDialogTests COMMAND LeyoChatRuntimeArchitectureDialogTests)

add_executable(LeyoChatContextPanelTests
    TestContextPanel.cpp
    ../src/ui/ContextPanel.cpp
)

target_include_directories(LeyoChatContextPanelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatContextPanelTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatContextPanelTests)

add_test(NAME LeyoChatContextPanelTests COMMAND LeyoChatContextPanelTests)

add_executable(LeyoChatGlobalSearchPanelTests
    TestGlobalSearchPanel.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/GlobalSearchPanel.cpp
    ../src/ui/GlobalSearchHistory.cpp
)

target_include_directories(LeyoChatGlobalSearchPanelTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGlobalSearchPanelTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatGlobalSearchPanelTests)

add_test(NAME LeyoChatGlobalSearchPanelTests COMMAND LeyoChatGlobalSearchPanelTests)

add_executable(LeyoChatWorkbenchPageTests
    TestWorkbenchPage.cpp
    ../src/ui/WorkbenchPage.cpp
)

target_include_directories(LeyoChatWorkbenchPageTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatWorkbenchPageTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatWorkbenchPageTests)

add_test(NAME LeyoChatWorkbenchPageTests COMMAND LeyoChatWorkbenchPageTests)

add_executable(LeyoChatInitialSetupPageTests
    TestInitialSetupPage.cpp
    ../src/app/AppSettings.cpp
    ../src/ui/InitialSetupPage.cpp
    ../src/ui/ClientAppearance.cpp
    ../src/ui/ClientPreferences.cpp
)

target_include_directories(LeyoChatInitialSetupPageTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatInitialSetupPageTests PRIVATE Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatInitialSetupPageTests)

add_test(NAME LeyoChatInitialSetupPageTests COMMAND LeyoChatInitialSetupPageTests)
set_tests_properties(LeyoChatInitialSetupPageTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatLoginDialogTests
    TestLoginDialog.cpp
    ../src/ui/LoginDialog.cpp
    ../src/services/IdentityService.cpp
    ../src/storage/ProfileRepository.cpp
    ../src/app/AppSettings.cpp
)

target_include_directories(LeyoChatLoginDialogTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatLoginDialogTests PRIVATE Qt6::Widgets Qt6::Sql Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatLoginDialogTests)

add_test(NAME LeyoChatLoginDialogTests COMMAND LeyoChatLoginDialogTests)
set_tests_properties(LeyoChatLoginDialogTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatMainWindowTests
    TestMainWindow.cpp
    ../src/app/AppSettings.cpp
    ../src/app/ApplicationInfo.cpp
    ../src/app/ImageFileTypeHelpers.cpp
    ../src/app/GroupFileServiceConfigResolver.cpp
    ../src/app/GroupFileSendRouting.cpp
    ../src/app/TestModeContext.cpp
    ../src/ui/MainWindow.cpp
    ../src/ui/ClientAppearance.cpp
    ../src/ui/ClientPreferences.cpp
    ../src/architecture/HybridRoutingPolicy.cpp
    ../src/architecture/RuntimeArchitectureQueryService.cpp
    ../src/ui/ContactListModel.cpp
    ../src/ui/ConversationListModel.cpp
    ../src/ui/MessageListModel.cpp
    ../src/ui/MessageDeliveryPresentation.cpp
    ../src/ui/ChatComposerWidget.cpp
    ../src/ui/ChatHeaderWidget.cpp
    ../src/ui/ChatWorkspaceWidget.cpp
    ../src/ui/ConversationHistoryCalendarWidget.cpp
    ../src/ui/ConversationHistoryDialog.cpp
    ../src/ui/ConversationSidebarWidget.cpp
    ../src/ui/ContextPanel.cpp
    ../src/ui/CloseToTrayDialog.cpp
    ../src/ui/ConnectIpDialog.cpp
    ../src/ui/CreateGroupDialog.cpp
    ../src/ui/ConversationsPage.cpp
    ../src/ui/DirectoryPage.cpp
    ../src/ui/NotificationsPage.cpp
    ../src/ui/KnowledgePage.cpp
    ../src/ui/GlobalSearchPanel.cpp
    ../src/ui/GlobalSearchHistory.cpp
    ../src/ui/GroupInfoPanel.cpp
    ../src/ui/SettingsPage.cpp
    ../src/ui/ForwardDetailDialog.cpp
    ../src/ui/ConversationItemWidget.cpp
    ../src/ui/ConversationCardDelegate.cpp
    ../src/ui/MessageBubbleWidget.cpp
    ../src/ui/MessageBubbleDelegate.cpp
    ../src/ui/MessageTextLinkifier.cpp
    ../src/ui/ReactionToolbar.cpp
    ../src/ui/MessageThumbnailCache.cpp
    ../src/ui/ContactCardDelegate.cpp
    ../src/ui/OrgTreeDelegate.cpp
    ../src/ui/GroupFileTableModel.cpp
    ../src/ui/GroupFileVersionDialog.cpp
    ../src/ui/GroupFileManagerDialog.cpp
    ../src/ui/ReminderListModel.cpp
    ../src/ui/PinyinHelper.cpp
    ../src/ui/AlphabetIndexBar.cpp
    ../src/ui/ScreenshotEditorWindow.cpp
    ../src/ui/StickerManager.cpp
    ../src/ui/FilePreviewWidget.cpp
    ../src/ui/MarkdownRenderer.cpp
    ../src/ui/CallWindow.cpp
    ../src/ui/ScreenshotOverlay.cpp
    ../src/ui/ImageViewerWidget.cpp
    ../src/ui/ProfileCardPopup.cpp
    ../src/ui/UpdateBar.cpp
    ../src/ui/WorkbenchPage.cpp
    ../src/ui/AiKnowledgePanel.cpp
    ../src/store/ChatDataStore.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/ui/TransferListModel.cpp
    ../src/architecture/RuntimeArchitecturePresentation.cpp
    ../src/integrations/AzureDevOpsSettings.cpp
    ../src/integrations/RemoteChatServiceSettings.cpp
    ../src/integrations/RemoteFileServiceSettings.cpp
    ../src/integrations/KnowledgeServiceSettings.cpp
    ../src/integrations/OutlookSettings.cpp
    ../src/integrations/HttpKnowServiceClient.cpp
    ../src/ui/GlobalHotkeyManager.cpp
    ../src/ui/KnowledgeServiceSettingsWidget.cpp
)

target_include_directories(LeyoChatMainWindowTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(LeyoChatMainWindowTests PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Multimedia Qt6::Test md4c ElaWidgetTools)
enable_project_warnings(LeyoChatMainWindowTests)

add_test(
    NAME LeyoChatMainWindowTests
    COMMAND powershell -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/RunQtTestFunctions.ps1"
            "$<TARGET_FILE:LeyoChatMainWindowTests>")

add_executable(LeyoChatStage2ExtensionIntegrationTests
    TestStage2ExtensionIntegration.cpp
    ../src/architecture/ArchitectureSnapshotAssembler.cpp
    ../src/architecture/DatabaseResourceCatalog.cpp
    ../src/architecture/DatabaseServiceBindingCatalog.cpp
    ../src/architecture/DatabaseServiceDiscoveryProvider.cpp
    ../src/architecture/DatabaseServiceSelectionCatalog.cpp
    ../src/architecture/PersistedRuntimeArchitectureLoader.cpp
    ../src/architecture/RuntimeArchitectureFacade.cpp
    ../src/architecture/RuntimeArchitectureQueryService.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/integrations/DevOpsAdapterContracts.cpp
    ../src/integrations/OutlookAdapterContracts.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/GroupService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
    ../src/storage/ServiceBindingRepository.cpp
    ../src/storage/ServiceRegistryRepository.cpp
    ../src/storage/ServiceResourceRepository.cpp
)

target_include_directories(LeyoChatStage2ExtensionIntegrationTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatStage2ExtensionIntegrationTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatStage2ExtensionIntegrationTests)

add_test(NAME LeyoChatStage2ExtensionIntegrationTests COMMAND LeyoChatStage2ExtensionIntegrationTests)

add_executable(LeyoChatConversationFlagsTests
    TestConversationFlags.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/GroupRepository.cpp
)

target_include_directories(LeyoChatConversationFlagsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatConversationFlagsTests PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatConversationFlagsTests)

add_test(NAME LeyoChatConversationFlagsTests COMMAND LeyoChatConversationFlagsTests)

add_executable(LeyoChatConversationRepositoryTests
    TestConversationRepository.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ConversationRepository.cpp
)
target_include_directories(LeyoChatConversationRepositoryTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatConversationRepositoryTests
    PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatConversationRepositoryTests)
add_test(NAME LeyoChatConversationRepositoryTests
    COMMAND LeyoChatConversationRepositoryTests)

add_executable(LeyoChatRemoteMessageEventCursorTests
    TestRemoteMessageEventCursor.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ConversationRepository.cpp
)
target_include_directories(LeyoChatRemoteMessageEventCursorTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteMessageEventCursorTests
    PRIVATE Qt6::Test Qt6::Sql)
enable_project_warnings(LeyoChatRemoteMessageEventCursorTests)
add_test(NAME LeyoChatRemoteMessageEventCursorTests
    COMMAND LeyoChatRemoteMessageEventCursorTests)

add_executable(LeyoChatAsyncFlushWorkerTests
    TestAsyncFlushWorker.cpp
    ../src/network/MessageCodec.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/FileTransferRepository.cpp
)
target_include_directories(LeyoChatAsyncFlushWorkerTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatAsyncFlushWorkerTests
    PRIVATE Qt6::Test Qt6::Sql Qt6::Concurrent)
enable_project_warnings(LeyoChatAsyncFlushWorkerTests)
add_test(NAME LeyoChatAsyncFlushWorkerTests
    COMMAND LeyoChatAsyncFlushWorkerTests)

add_executable(LeyoChatRemoteFileServiceContractsTests
    TestRemoteFileServiceContracts.cpp
    ../src/integrations/RemoteFileServiceContracts.cpp
    ../src/integrations/RemoteFileServiceSettings.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/app/AppSettings.cpp
)
target_include_directories(LeyoChatRemoteFileServiceContractsTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteFileServiceContractsTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatRemoteFileServiceContractsTests)
add_test(NAME LeyoChatRemoteFileServiceContractsTests
    COMMAND LeyoChatRemoteFileServiceContractsTests)

add_executable(LeyoChatRemoteFileServiceAdapterTests
    TestRemoteFileServiceAdapter.cpp
    ../src/integrations/RemoteFileServiceAdapter.cpp
    ../src/integrations/RemoteFileServiceContracts.cpp
    ../src/integrations/RemoteFileServiceSettings.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/app/AppSettings.cpp
)
target_include_directories(LeyoChatRemoteFileServiceAdapterTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteFileServiceAdapterTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatRemoteFileServiceAdapterTests)
add_test(NAME LeyoChatRemoteFileServiceAdapterTests
    COMMAND LeyoChatRemoteFileServiceAdapterTests)

add_executable(LeyoChatRemoteChatServiceSettingsTests
    TestRemoteChatServiceSettings.cpp
    ../src/integrations/RemoteChatServiceSettings.cpp
    ../src/app/AppSettings.cpp
)
target_include_directories(LeyoChatRemoteChatServiceSettingsTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteChatServiceSettingsTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatRemoteChatServiceSettingsTests)
add_test(NAME LeyoChatRemoteChatServiceSettingsTests
    COMMAND LeyoChatRemoteChatServiceSettingsTests)

add_executable(LeyoChatTransportPolicyTests
    TestTransportPolicy.cpp
    ../src/services/TransportPolicy.cpp
)
target_include_directories(LeyoChatTransportPolicyTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatTransportPolicyTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatTransportPolicyTests)
add_test(NAME LeyoChatTransportPolicyTests
    COMMAND LeyoChatTransportPolicyTests)

add_executable(LeyoChatP2PConnectionPolicyTests
    TestP2PConnectionPolicy.cpp
    ../src/services/P2PConnectionPolicy.cpp
)
target_include_directories(LeyoChatP2PConnectionPolicyTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatP2PConnectionPolicyTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatP2PConnectionPolicyTests)
add_test(NAME LeyoChatP2PConnectionPolicyTests
    COMMAND LeyoChatP2PConnectionPolicyTests)

add_executable(LeyoChatMessageSessionRegistryTests
    TestMessageSessionRegistry.cpp
    ../src/fileservice/MessageSessionRegistry.cpp
)
target_include_directories(LeyoChatMessageSessionRegistryTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageSessionRegistryTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatMessageSessionRegistryTests)
add_test(NAME LeyoChatMessageSessionRegistryTests
    COMMAND LeyoChatMessageSessionRegistryTests)

add_executable(LeyoChatMessageServiceStoreBoundariesTests
    TestMessageServiceStoreBoundaries.cpp
    ../src/fileservice/MessageServiceDatabase.cpp
    ../src/fileservice/MessageServiceOperations.cpp
    ../src/fileservice/MessageServiceStoreBoundaries.cpp
    ../src/fileservice/MessageSessionRegistry.cpp
)
target_include_directories(LeyoChatMessageServiceStoreBoundariesTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageServiceStoreBoundariesTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatMessageServiceStoreBoundariesTests)
add_test(NAME LeyoChatMessageServiceStoreBoundariesTests
    COMMAND LeyoChatMessageServiceStoreBoundariesTests)

add_executable(LeyoChatServerMessageClientTests
    TestServerMessageClient.cpp
    ../src/integrations/ServerMessageClient.cpp
)
target_include_directories(LeyoChatServerMessageClientTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatServerMessageClientTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_project_warnings(LeyoChatServerMessageClientTests)
add_test(NAME LeyoChatServerMessageClientTests
    COMMAND LeyoChatServerMessageClientTests)

add_executable(LeyoChatRecipientCapabilityResolverTests
    TestRecipientCapabilityResolver.cpp
    ../src/services/RecipientCapabilityResolver.cpp
)
target_include_directories(LeyoChatRecipientCapabilityResolverTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRecipientCapabilityResolverTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatRecipientCapabilityResolverTests)
add_test(NAME LeyoChatRecipientCapabilityResolverTests
    COMMAND LeyoChatRecipientCapabilityResolverTests)

add_executable(LeyoChatMessageSyncServiceTests
    TestMessageSyncService.cpp
    ../src/integrations/ServerMessageClient.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/MessageSyncService.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
)
target_include_directories(LeyoChatMessageSyncServiceTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatMessageSyncServiceTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatMessageSyncServiceTests)
add_test(NAME LeyoChatMessageSyncServiceTests
    COMMAND LeyoChatMessageSyncServiceTests)

add_executable(LeyoChatRemoteMessageSyncCoordinatorTests
    TestRemoteMessageSyncCoordinator.cpp
    ../src/integrations/ServerMessageClient.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/MessageSyncService.cpp
    ../src/services/RemoteMessageSyncCoordinator.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
)
target_include_directories(LeyoChatRemoteMessageSyncCoordinatorTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteMessageSyncCoordinatorTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatRemoteMessageSyncCoordinatorTests)
add_test(NAME LeyoChatRemoteMessageSyncCoordinatorTests
    COMMAND LeyoChatRemoteMessageSyncCoordinatorTests)

add_executable(LeyoChatRemoteMessageEventConsumerTests
    TestRemoteMessageEventConsumer.cpp
    ../src/integrations/ServerMessageClient.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/MessageSyncService.cpp
    ../src/services/RemoteMessageEventConsumer.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
)
target_include_directories(LeyoChatRemoteMessageEventConsumerTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteMessageEventConsumerTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatRemoteMessageEventConsumerTests)
add_test(NAME LeyoChatRemoteMessageEventConsumerTests
    COMMAND LeyoChatRemoteMessageEventConsumerTests)

add_executable(LeyoChatRemoteMessageDeviceIdentityTests
    TestRemoteMessageDeviceIdentity.cpp
    ../src/services/RemoteMessageDeviceIdentity.cpp
)
target_include_directories(LeyoChatRemoteMessageDeviceIdentityTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatRemoteMessageDeviceIdentityTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatRemoteMessageDeviceIdentityTests)
add_test(NAME LeyoChatRemoteMessageDeviceIdentityTests
    COMMAND LeyoChatRemoteMessageDeviceIdentityTests)

add_executable(LeyoChatReliableDirectMessageSenderTests
    TestReliableDirectMessageSender.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/ChatService.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/ReliableDirectMessageSender.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/services/TransportPolicy.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/ui/StickerManager.cpp
)
target_include_directories(LeyoChatReliableDirectMessageSenderTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReliableDirectMessageSenderTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test Qt6::Widgets)
enable_project_warnings(LeyoChatReliableDirectMessageSenderTests)
add_test(NAME LeyoChatReliableDirectMessageSenderTests
    COMMAND LeyoChatReliableDirectMessageSenderTests)

add_executable(LeyoChatDirectMessageP2PIntegrationTests
    TestDirectMessageP2PIntegration.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/network/MessageCodec.cpp
    ../src/network/PeerConnection.cpp
    ../src/network/TlsHelper.cpp
    ../src/services/ChatService.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/ReliableDirectMessageSender.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/services/TransportPolicy.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/ui/StickerManager.cpp
)
target_include_directories(LeyoChatDirectMessageP2PIntegrationTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatDirectMessageP2PIntegrationTests
    PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test Qt6::Widgets)
enable_project_warnings(LeyoChatDirectMessageP2PIntegrationTests)
add_test(NAME LeyoChatDirectMessageP2PIntegrationTests
    COMMAND LeyoChatDirectMessageP2PIntegrationTests)

add_executable(LeyoChatReliableDirectEnvelopeSenderTests
    TestReliableDirectEnvelopeSender.cpp
    ../src/services/ReliableDirectEnvelopeSender.cpp
    ../src/services/TransportPolicy.cpp
)
target_include_directories(LeyoChatReliableDirectEnvelopeSenderTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReliableDirectEnvelopeSenderTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatReliableDirectEnvelopeSenderTests)
add_test(NAME LeyoChatReliableDirectEnvelopeSenderTests
    COMMAND LeyoChatReliableDirectEnvelopeSenderTests)

add_executable(LeyoChatApplicationRoutingSourceTests
    TestLeyoApplicationRoutingSource.cpp
)
target_compile_definitions(LeyoChatApplicationRoutingSourceTests PRIVATE
    LEYOCHAT_APP_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/app/LeyoApplication.cpp")
target_include_directories(LeyoChatApplicationRoutingSourceTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatApplicationRoutingSourceTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatApplicationRoutingSourceTests)
add_test(NAME LeyoChatApplicationRoutingSourceTests
    COMMAND LeyoChatApplicationRoutingSourceTests)

add_executable(LeyoChatSynchronousNetworkReplyLifetimeSourceTests
    TestSynchronousNetworkReplyLifetimeSource.cpp
)
target_compile_definitions(LeyoChatSynchronousNetworkReplyLifetimeSourceTests PRIVATE
    LEYOCHAT_SYNC_REPLY_HEADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/integrations/SyncNetworkReply.h"
    LEYOCHAT_SERVER_MESSAGE_CLIENT_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/integrations/ServerMessageClient.cpp"
    LEYOCHAT_REMOTE_FILE_SERVICE_ADAPTER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/integrations/RemoteFileServiceAdapter.cpp"
    LEYOCHAT_OUTLOOK_EWS_TRANSPORT_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/integrations/OutlookEwsTransport.cpp"
    LEYOCHAT_ONLYOFFICE_CALLBACK_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice/OnlyOfficeCallbackHandler.cpp")
target_link_libraries(LeyoChatSynchronousNetworkReplyLifetimeSourceTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatSynchronousNetworkReplyLifetimeSourceTests)
add_test(NAME LeyoChatSynchronousNetworkReplyLifetimeSourceTests
    COMMAND LeyoChatSynchronousNetworkReplyLifetimeSourceTests)

add_executable(LeyoChatReliableGroupMessageSenderTests
    TestReliableGroupMessageSender.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/ChatService.cpp
    ../src/services/DirectConversationAddressing.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/ReliableGroupMessageSender.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/services/TransportPolicy.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/ui/StickerManager.cpp
)
target_include_directories(LeyoChatReliableGroupMessageSenderTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReliableGroupMessageSenderTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test Qt6::Widgets)
enable_project_warnings(LeyoChatReliableGroupMessageSenderTests)
add_test(NAME LeyoChatReliableGroupMessageSenderTests
    COMMAND LeyoChatReliableGroupMessageSenderTests)

add_executable(LeyoChatReliableGroupEnvelopeSenderTests
    TestReliableGroupEnvelopeSender.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/ReliableGroupEnvelopeSender.cpp
    ../src/services/TransportPolicy.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
)
target_include_directories(LeyoChatReliableGroupEnvelopeSenderTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReliableGroupEnvelopeSenderTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatReliableGroupEnvelopeSenderTests)
add_test(NAME LeyoChatReliableGroupEnvelopeSenderTests
    COMMAND LeyoChatReliableGroupEnvelopeSenderTests)

add_executable(LeyoChatReliableGroupFileMessageSenderTests
    TestReliableGroupFileMessageSender.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/ReliableGroupFileMessageSender.cpp
    ../src/services/TransportPolicy.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
)
target_include_directories(LeyoChatReliableGroupFileMessageSenderTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatReliableGroupFileMessageSenderTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatReliableGroupFileMessageSenderTests)
add_test(NAME LeyoChatReliableGroupFileMessageSenderTests
    COMMAND LeyoChatReliableGroupFileMessageSenderTests)

add_executable(LeyoChatGroupFanOutDeliveryTests
    TestGroupFanOutDelivery.cpp
    ../src/app/GroupFanOutDelivery.cpp
)
target_include_directories(LeyoChatGroupFanOutDeliveryTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupFanOutDeliveryTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatGroupFanOutDeliveryTests)
add_test(NAME LeyoChatGroupFanOutDeliveryTests
    COMMAND LeyoChatGroupFanOutDeliveryTests)

add_executable(LeyoChatFileServiceDatabaseTests
    TestFileServiceDatabase.cpp
    ../src/fileservice/FileServiceDatabase.cpp
)
target_include_directories(LeyoChatFileServiceDatabaseTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatFileServiceDatabaseTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileServiceDatabaseTests)
add_test(NAME LeyoChatFileServiceDatabaseTests
    COMMAND LeyoChatFileServiceDatabaseTests)

add_executable(LeyoChatMessageServiceDatabaseTests
    TestMessageServiceDatabase.cpp
    ../src/fileservice/MessageServiceDatabase.cpp
)
target_include_directories(LeyoChatMessageServiceDatabaseTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatMessageServiceDatabaseTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatMessageServiceDatabaseTests)
add_test(NAME LeyoChatMessageServiceDatabaseTests
    COMMAND LeyoChatMessageServiceDatabaseTests)

add_executable(LeyoChatServiceConfigTests
    TestLeyoChatServiceConfig.cpp
    ../src/chatservice/ServiceConfig.cpp
)
target_include_directories(LeyoChatServiceConfigTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatServiceConfigTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatServiceConfigTests)
add_test(NAME LeyoChatServiceConfigTests
    COMMAND LeyoChatServiceConfigTests)

add_executable(LeyoChatServiceHostTests
    TestLeyoChatServiceHost.cpp
    ../src/chatservice/ServiceHostPolicy.cpp
)
target_include_directories(LeyoChatServiceHostTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatServiceHostTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatServiceHostTests)
add_test(NAME LeyoChatServiceHostTests
    COMMAND LeyoChatServiceHostTests)

add_executable(LeyoChatMessageServiceHttpContractsTests
    TestMessageServiceHttpContracts.cpp
    ../src/fileservice/MessageServiceHttpContracts.cpp
    ../src/fileservice/MessageServiceDatabase.cpp
)
target_include_directories(LeyoChatMessageServiceHttpContractsTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatMessageServiceHttpContractsTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatMessageServiceHttpContractsTests)
add_test(NAME LeyoChatMessageServiceHttpContractsTests
    COMMAND LeyoChatMessageServiceHttpContractsTests)

add_executable(LeyoChatFileServiceAuthTests
    TestFileServiceAuth.cpp
    ../src/fileservice/FileServiceAuth.cpp
    ../src/fileservice/FileServiceDatabase.cpp
)
target_include_directories(LeyoChatFileServiceAuthTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatFileServiceAuthTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileServiceAuthTests)
add_test(NAME LeyoChatFileServiceAuthTests
    COMMAND LeyoChatFileServiceAuthTests)

add_executable(LeyoChatFileServiceHttpServerTests
    TestFileServiceHttpServer.cpp
    ../src/fileservice/FileServiceAuth.cpp
    ../src/fileservice/FileServiceDatabase.cpp
    ../src/fileservice/FileServiceHttpServer.cpp
    ../src/fileservice/FileStorageManager.cpp
    ../src/fileservice/WopiHandler.cpp
    ../src/fileservice/OnlyOfficeCallbackHandler.cpp
)
target_include_directories(LeyoChatFileServiceHttpServerTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatFileServiceHttpServerTests
    PRIVATE Qt6::Core Qt6::Concurrent Qt6::HttpServer Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileServiceHttpServerTests)
add_test(NAME LeyoChatFileServiceHttpServerTests
    COMMAND LeyoChatFileServiceHttpServerTests)

add_executable(LeyoChatMessageServiceHttpServerTests
    TestMessageServiceHttpServer.cpp
    ../src/fileservice/FileServiceAuth.cpp
    ../src/fileservice/FileServiceDatabase.cpp
    ../src/fileservice/FileServiceHttpServer.cpp
    ../src/fileservice/FileStorageManager.cpp
    ../src/fileservice/MessageEventBus.cpp
    ../src/fileservice/MessageServiceDatabase.cpp
    ../src/fileservice/MessageServiceHttpContracts.cpp
    ../src/fileservice/MessageServiceOperations.cpp
    ../src/fileservice/MessageSessionRegistry.cpp
    ../src/fileservice/MessageServiceHttpRoutes.cpp
    ../src/fileservice/WopiHandler.cpp
    ../src/fileservice/OnlyOfficeCallbackHandler.cpp
    ../src/integrations/ServerMessageClient.cpp
)
target_include_directories(LeyoChatMessageServiceHttpServerTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatMessageServiceHttpServerTests
    PRIVATE Qt6::Core Qt6::Concurrent Qt6::HttpServer Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatMessageServiceHttpServerTests)
add_test(NAME LeyoChatMessageServiceHttpServerTests
    COMMAND LeyoChatMessageServiceHttpServerTests)

add_executable(LeyoChatSharedFileResourceSyncTests
    TestSharedFileResourceSync.cpp
    ../src/app/AppSettings.cpp
    ../src/architecture/ArchitectureSnapshotAssembler.cpp
    ../src/architecture/DatabaseResourceCatalog.cpp
    ../src/architecture/DatabaseServiceBindingCatalog.cpp
    ../src/architecture/DatabaseServiceDiscoveryProvider.cpp
    ../src/architecture/DatabaseServiceSelectionCatalog.cpp
    ../src/architecture/PersistedRuntimeArchitectureLoader.cpp
    ../src/architecture/RuntimeArchitectureFacade.cpp
    ../src/architecture/RuntimeArchitectureQueryService.cpp
    ../src/integrations/RemoteFileServiceSettings.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/SharedFileResourceSync.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/ServiceBindingRepository.cpp
    ../src/storage/ServiceRegistryRepository.cpp
    ../src/storage/ServiceResourceRepository.cpp
)
target_include_directories(LeyoChatSharedFileResourceSyncTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatSharedFileResourceSyncTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatSharedFileResourceSyncTests)
add_test(NAME LeyoChatSharedFileResourceSyncTests
    COMMAND LeyoChatSharedFileResourceSyncTests)

add_executable(LeyoChatGroupFileTransferTests
    TestGroupFileTransfer.cpp
    ../src/architecture/ResourceReferenceMessage.cpp
    ../src/fileservice/FileServiceDatabase.cpp
    ../src/fileservice/FileStorageManager.cpp
    ../src/integrations/SharedFileResourceContracts.cpp
    ../src/network/MessageCodec.cpp
    ../src/services/FileTransferService.cpp
    ../src/services/GroupService.cpp
    ../src/services/OutgoingFileSnapshot.cpp
    ../src/services/ResourceRefRouter.cpp
    ../src/store/ChatDataStore.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/storage/FileTransferRepository.cpp
    ../src/storage/GroupRepository.cpp
    ../src/ui/MessageListModel.cpp
    ../src/ui/MessageDeliveryPresentation.cpp
)
target_include_directories(LeyoChatGroupFileTransferTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupFileTransferTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(LeyoChatGroupFileTransferTests)
add_test(NAME LeyoChatGroupFileTransferTests
    COMMAND LeyoChatGroupFileTransferTests)

add_test(
    NAME LeyoChatPackagingTests
    COMMAND powershell -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/TestPackageWindows.ps1"
            -RepoRoot "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

get_filename_component(_mingw_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)

set_tests_properties(LeyoChatTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatResourceRefRouterTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupFileSendRoutingTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDevOpsAdapterContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsSettingsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsLinkParserTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsNotificationContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsInsertDialogTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsNotificationDialogTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsNotificationDispatcherTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAzureDevOpsBuildNotificationPollerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookAdapterContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookSettingsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookEwsTransportTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookNotificationContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookNotificationDialogTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookNotificationDispatcherTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookNotificationPollerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Gui>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatOutlookIntegrationHealthTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Gui>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatSharedFileResourceContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatApplicationInfoTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatInstallerLaunchPolicyTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatUpdateCheckerTests
    TestUpdateChecker.cpp
    ../src/update/UpdateChecker.cpp
    ../src/app/ApplicationInfo.cpp
)
target_include_directories(LeyoChatUpdateCheckerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(LeyoChatUpdateCheckerTests PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
enable_project_warnings(LeyoChatUpdateCheckerTests)
add_test(NAME LeyoChatUpdateCheckerTests COMMAND LeyoChatUpdateCheckerTests
)
set_tests_properties(LeyoChatUpdateCheckerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Concurrent>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Test>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatIntegrationNotificationPresentationTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatIntegrationPollBackoffTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatTransferSpeedTrackerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatUiRestoreHelpersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatImageFileTypeHelpersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessagePresentationHelpersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAttachmentPayloadHelpersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatPeerPresentationHelpersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatContactListModelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatPeerDirectoryServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageListModelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageBubbleWidgetTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatPresenceHeuristicsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatChatServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatIdentityTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDatabaseTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupRepositoryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupResourceRefFanOutTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileTransferTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderRepositoryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderDialogTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderSignalContractTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderNotificationPresentationTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderActionRoutingTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderListModelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReminderManagementSurfaceContractTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGlobalSearchPanelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(
    LeyoChatRuntimeArchitectureDialogTests
    LeyoChatContextPanelTests
    LeyoChatWorkbenchPageTests
    PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileTransferServiceSourceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileTransferServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDelegateTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatChatHeaderWidgetTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatChatComposerWidgetTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatLocalAzureDevOpsAdapterTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatTransferListModelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupInfoPanelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;QT_QPA_FONTDIR=set:$ENV{WINDIR}/Fonts;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set(_leyochat_mainwindow_test_path
    "${_mingw_bin_dir}\;$<TARGET_FILE_DIR:ElaWidgetTools>\;$<TARGET_FILE_DIR:Qt6::Widgets>\;$<TARGET_FILE_DIR:Qt6::Network>\;$<TARGET_FILE_DIR:Qt6::Sql>\;$ENV{PATH}")
set_property(TEST LeyoChatMainWindowTests PROPERTY ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen"
    "PATH=${_leyochat_mainwindow_test_path}"
)

set_tests_properties(LeyoChatStage2ExtensionIntegrationTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatConversationFlagsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatConversationRepositoryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteMessageEventCursorTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatArchitectureReadinessTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileServiceAuthTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileServiceHttpServerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::HttpServer>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageServiceHttpServerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::HttpServer>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatSharedFileResourceSyncTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDiagnosticsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDevelopmentTestModeTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatLanDiscoveryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatNetworkTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatAsyncFlushWorkerTests PROPERTIES
    ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteFileServiceContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteFileServiceAdapterTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteChatServiceSettingsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatTransportPolicyTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatP2PConnectionPolicyTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageSessionRegistryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageServiceStoreBoundariesTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatServerMessageClientTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRecipientCapabilityResolverTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageSyncServiceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteMessageSyncCoordinatorTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteMessageEventConsumerTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatRemoteMessageDeviceIdentityTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReliableDirectMessageSenderTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatDirectMessageP2PIntegrationTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReliableDirectEnvelopeSenderTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatApplicationRoutingSourceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatSynchronousNetworkReplyLifetimeSourceTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReliableGroupMessageSenderTests PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReliableGroupEnvelopeSenderTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatReliableGroupFileMessageSenderTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupFanOutDeliveryTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileServiceDatabaseTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageServiceDatabaseTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatServiceConfigTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatServiceHostTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatMessageServiceHttpContractsTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatFileServiceAuthTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

set_tests_properties(LeyoChatGroupFileTransferTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatFileServiceDatabaseFoldersTests
    TestFileServiceDatabaseFolders.cpp
    ../src/fileservice/FileServiceDatabase.cpp
)
target_include_directories(LeyoChatFileServiceDatabaseFoldersTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatFileServiceDatabaseFoldersTests
    PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileServiceDatabaseFoldersTests)
add_test(NAME LeyoChatFileServiceDatabaseFoldersTests
    COMMAND LeyoChatFileServiceDatabaseFoldersTests)

set_tests_properties(LeyoChatFileServiceDatabaseFoldersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatFileServiceHttpServerFoldersTests
    TestFileServiceHttpServerFolders.cpp
    ../src/fileservice/FileServiceAuth.cpp
    ../src/fileservice/FileServiceDatabase.cpp
    ../src/fileservice/FileServiceHttpServer.cpp
    ../src/fileservice/FileStorageManager.cpp
    ../src/fileservice/WopiHandler.cpp
    ../src/fileservice/OnlyOfficeCallbackHandler.cpp
)
target_include_directories(LeyoChatFileServiceHttpServerFoldersTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatFileServiceHttpServerFoldersTests
    PRIVATE Qt6::Core Qt6::Concurrent Qt6::HttpServer Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatFileServiceHttpServerFoldersTests)
add_test(NAME LeyoChatFileServiceHttpServerFoldersTests
    COMMAND LeyoChatFileServiceHttpServerFoldersTests)

set_tests_properties(LeyoChatFileServiceHttpServerFoldersTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Concurrent>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::HttpServer>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatGroupFileTableModelTests
    TestGroupFileTableModel.cpp
    ../src/ui/GroupFileTableModel.cpp
)
target_include_directories(LeyoChatGroupFileTableModelTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(LeyoChatGroupFileTableModelTests
    PRIVATE Qt6::Core Qt6::Test)
enable_project_warnings(LeyoChatGroupFileTableModelTests)
add_test(NAME LeyoChatGroupFileTableModelTests
    COMMAND LeyoChatGroupFileTableModelTests)

set_tests_properties(LeyoChatGroupFileTableModelTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(LeyoChatWopiTokensTests
    TestWopiTokens.cpp
    ../src/fileservice/FileServiceAuth.cpp
    ../src/fileservice/FileServiceDatabase.cpp
    ../src/fileservice/FileServiceHttpServer.cpp
    ../src/fileservice/FileStorageManager.cpp
    ../src/fileservice/WopiHandler.cpp
    ../src/fileservice/OnlyOfficeCallbackHandler.cpp
)
target_include_directories(LeyoChatWopiTokensTests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fileservice)
target_link_libraries(LeyoChatWopiTokensTests
    PRIVATE Qt6::Core Qt6::Concurrent Qt6::HttpServer Qt6::Network Qt6::Sql Qt6::Test)
enable_project_warnings(LeyoChatWopiTokensTests)
add_test(NAME LeyoChatWopiTokensTests
    COMMAND LeyoChatWopiTokensTests)

set_tests_properties(LeyoChatWopiTokensTests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Concurrent>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::HttpServer>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Network>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(TestChatDataStore
    TestChatDataStore.cpp
    ../src/store/ChatDataStore.cpp
    ../src/ui/PinyinHelper.cpp
)
target_include_directories(TestChatDataStore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(TestChatDataStore PRIVATE Qt6::Core Qt6::Sql Qt6::Widgets Qt6::Test ElaWidgetTools)
enable_project_warnings(TestChatDataStore)
add_test(NAME TestChatDataStore COMMAND TestChatDataStore)
set_tests_properties(TestChatDataStore PROPERTIES
    ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:offscreen;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Widgets>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:ElaWidgetTools>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)

add_executable(TestDatabaseWorker
    TestDatabaseWorker.cpp
    ../src/network/MessageCodec.cpp
    ../src/store/DatabaseWorker.cpp
    ../src/storage/ConversationRepository.cpp
    ../src/storage/GroupRepository.cpp
    ../src/storage/DatabaseManager.cpp
    ../src/services/MessageMutationService.cpp
    ../src/services/DirectConversationAddressing.cpp
)
target_include_directories(TestDatabaseWorker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(TestDatabaseWorker PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
enable_project_warnings(TestDatabaseWorker)
add_test(NAME TestDatabaseWorker COMMAND TestDatabaseWorker)
set_tests_properties(TestDatabaseWorker PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Sql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${_mingw_bin_dir}"
)
