# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui GuiPrivate WebEngineCore WebEngineWidgets Network Concurrent Xml)

# Process Qt resources
qt6_add_resources(RobloxStudio_RESOURCES RobloxStudio.qrc)

# Add SARibbon subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/sa-ribbon SARibbon_build)

# Disable Qt's signals, slots, emit keywords to avoid conflicts
# we make sure its done after SARibbon, as that codebase uses these heavily!
add_definitions(-DQT_NO_KEYWORDS)

find_package(SDL3 REQUIRED)

add_definitions(-DRBX_STUDIO_BUILD)

# include the core modules
include(App)
include(Network)
include(Rendering)
include(ClientShared)
include(ClientBase)
include(CSG)

add_subdirectory(${CMAKE_SOURCE_DIR}/engine/csg CSG)

# Define the executable for RobloxStudio
add_executable(RobloxStudio
    # RobloxStudio sources
    AddEmulationDeviceDialog.cpp
    AppleCrashReporter.cpp
    AuthenticationHelper.cpp
    AuthoringSettings.cpp
    AutoSaveDialog.cpp
    CSGOperations.cpp
    CloudEditAdornable.cpp
    CommonInsertWidget.cpp
    DebuggerClient.cpp
    DebuggerWidgets.cpp
    DocDockManager.cpp
    DocDockWidget.cpp
    DocTabManager.cpp
    ExternalHandlers.cpp
    FindDialog.cpp
    FunctionMarshaller.cpp
    GalleryItemColor.cpp
    InsertObjectListWidget.cpp
    InsertObjectListWidgetItem.cpp
    InsertServiceDialog.cpp
    KeepSavedChangesDialog.cpp
    LuaSourceBuffer.cpp
    MachineConfig.cpp
    ManageEmulationDeviceDialog.cpp
    MobileDevelopmentDeployer.cpp
    NameValueStoreManager.cpp
    PairRbxDeviceDialog.cpp
    PlayersDataManager.cpp
    PropertyItems.cpp
    QtUtilities.cpp
    RbxContent.cpp
    RbxWorkspace.cpp
    RenderStatsItem.cpp
    Roblox.cpp
    RobloxApplicationManager.cpp
    RobloxBasicDoc.cpp
    RobloxBrowser.cpp
    RobloxChatWidget.cpp
    RobloxContextualHelp.cpp
    RobloxCookieJar.cpp
    RobloxCustomWidgets.cpp
    RobloxDiagnosticsView.cpp
    RobloxDocManager.cpp
    RobloxFindWidget.cpp
    RobloxGameExplorer.cpp
    RobloxIDEDoc.cpp
    RobloxInputConfigDialog.cpp
    RobloxKeyboardConfig.cpp
    RobloxMainWindow.cpp
    RobloxMouseConfig.cpp
    RobloxNetworkAccessManager.cpp
    RobloxNetworkReply.cpp
    RobloxObjectBrowserDoc.cpp
    RobloxPluginHost.cpp
    RobloxPrizeAwarder.cpp
    RobloxPropertyWidget.cpp
    RobloxQuickAccessConfig.cpp
    RobloxReportView.cpp
    RobloxRibbonMainWindow.cpp
    RobloxSavingStateDialog.cpp
    RobloxScriptDoc.cpp
    RobloxScriptReview.cpp
    RobloxSettings.cpp
    RobloxSettingsDialog.cpp
    RobloxStudioVerbs.cpp
    RobloxTaskScheduler.cpp
    RobloxTeamCreateWidget.cpp
    RobloxTextOutputWidget.cpp
    RobloxToolBox.cpp
    RobloxTreeWidget.cpp
    RobloxTutorials.cpp
    RobloxUser.cpp
    RobloxView.cpp
    RobloxWebDoc.cpp
    RobloxWebPage.cpp
    ScriptAnalysisWidget.cpp
    ScriptComboBox.cpp
    ScriptPickerDialog.cpp
    ScriptSideWidget.cpp
    ScriptSyntaxHighlighter.cpp
    ScriptTextEditor.cpp
    SelectionHighlightAdornable.cpp
    ShortcutHelpDialog.cpp
    SplineEditor.cpp
    StudioAnalytics.cpp
    StudioDeviceEmulator.cpp
    StudioIntellesense.cpp
    StudioSerializerHelper.cpp
    StudioUtilities.cpp
    UpdateUIManager.cpp
    UserInput.cpp
    UserInputUtil.cpp
    WebDialog.cpp
    filewatcher/FileWatcher.cpp
    filewatcher/FileWatcherLinux.cpp
    filewatcher/FileWatcherOSX.cpp
    filewatcher/FileWatcherWin32.cpp
    main.cpp
    ogrewidget.cpp
    RobloxRenderWindow.cpp
    ${RobloxStudio_RESOURCES}

    RibbonGroupedButton.cpp
    RibbonButtonGroup.cpp

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMDummy.cpp

    ${CLIENT_BASE_DIR}/MachineConfiguration.cpp
    ${CLIENT_BASE_DIR}/ReflectionMetadata.cpp
    ${CLIENT_BASE_DIR}/RenderSettingsItem.cpp


    ${CLIENT_SHARED_DIR}/DumpErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/ErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/LogManager.cpp
    ${CLIENT_SHARED_DIR}/VersionInfo.cpp
    ${CLIENT_SHARED_DIR}/CountersClient.cpp
    ${CLIENT_SHARED_DIR}/SDLGameController.cpp

    ${CLIENT_SHARED_DIR}/SharedLauncher.cpp

    ${CLIENT_SHARED_DIR}/AuthenticationMarshallar.cpp

)

# Link directly to individual object libraries instead of the combined libroblox
target_link_libraries(RobloxStudio PRIVATE
    # Qt6 libraries
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Xml

    # SARibbon library
    SARibbonBar

    # External dependencies
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Boost::chrono
    Boost::date_time
    Boost::atomic
    Boost::iostreams
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    GLEW::GLEW
    OpenGL::GL
    Threads::Threads
    Freetype::Freetype
    lz4::lz4
    PNG::PNG
    JPEG::JPEG
    ZLIB::ZLIB
    SDL3::SDL3
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"
    $<$<PLATFORM_ID:Linux>:sgCore>

    # Core modules
    $<TARGET_OBJECTS:Base>
    $<TARGET_OBJECTS:App_STUDIO>
    $<TARGET_OBJECTS:Network_STUDIO>
    $<TARGET_OBJECTS:BulletPhysics>
    $<TARGET_OBJECTS:G3D>
    $<TARGET_OBJECTS:AppDraw>
    $<TARGET_OBJECTS:GfxBase>
    $<TARGET_OBJECTS:GfxCore_STUDIO>
    $<TARGET_OBJECTS:GfxRender>
    $<TARGET_OBJECTS:CSG>
)

# Set Qt5-specific properties
set_target_properties(RobloxStudio PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)
