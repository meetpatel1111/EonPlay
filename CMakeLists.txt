cmake_minimum_required(VERSION 3.20)
project(EonPlay VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cross-platform compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /permissive-")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets Network Sql Gui)

# Enable Qt MOC, UIC, and RCC early
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Qt6 standard project setup
qt6_standard_project_setup()

# Find optional Qt WebSockets component
find_package(Qt6 QUIET COMPONENTS WebSockets)

# Find optional Qt components
if(NOT WIN32)
    find_package(Qt6 COMPONENTS DBus)
endif()

# Find libVLC
if(WIN32)
    # Windows libVLC configuration
    find_path(LIBVLC_INCLUDE_DIR vlc/vlc.h
        HINTS
        ${CMAKE_SOURCE_DIR}/third_party/vlc/include
        "C:/Program Files/VideoLAN/VLC/sdk/include"
        "C:/Program Files (x86)/VideoLAN/VLC/sdk/include"
    )
    
    find_library(LIBVLC_LIBRARY
        NAMES vlc libvlc
        HINTS
        ${CMAKE_SOURCE_DIR}/third_party/vlc/lib
        "C:/Program Files/VideoLAN/VLC/sdk/lib"
        "C:/Program Files (x86)/VideoLAN/VLC/sdk/lib"
    )
    
    find_library(LIBVLCCORE_LIBRARY
        NAMES vlccore libvlccore
        HINTS
        ${CMAKE_SOURCE_DIR}/third_party/vlc/lib
        "C:/Program Files/VideoLAN/VLC/sdk/lib"
        "C:/Program Files (x86)/VideoLAN/VLC/sdk/lib"
    )
elseif(APPLE)
    # macOS libVLC configuration
    find_package(PkgConfig REQUIRED)
    
    # Try pkg-config first
    pkg_check_modules(LIBVLC libvlc>=3.0.0)
    
    if(NOT LIBVLC_FOUND)
        # Fallback to manual search in common macOS locations
        find_path(LIBVLC_INCLUDE_DIR vlc/vlc.h
            PATHS
            /Applications/VLC.app/Contents/MacOS/include
            /opt/homebrew/include
            /usr/local/include
        )
        
        find_library(LIBVLC_LIBRARY
            NAMES vlc libvlc
            PATHS
            /Applications/VLC.app/Contents/MacOS/lib
            /opt/homebrew/lib
            /usr/local/lib
        )
        
        if(LIBVLC_INCLUDE_DIR AND LIBVLC_LIBRARY)
            set(LIBVLC_FOUND TRUE)
            set(LIBVLC_LIBRARIES ${LIBVLC_LIBRARY})
            set(LIBVLC_INCLUDE_DIRS ${LIBVLC_INCLUDE_DIR})
        endif()
    endif()
    
    if(NOT LIBVLC_FOUND)
        message(FATAL_ERROR "libVLC not found. Please install VLC: brew install vlc")
    endif()
else()
    # Linux libVLC configuration
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVLC REQUIRED libvlc>=3.0.0)
endif()

# Note: Include directories are now set per target using target_include_directories

# Source files
set(CORE_SOURCES
    src/core/EonPlayApplication.cpp
    src/core/ComponentManager.cpp
    src/core/EventBus.cpp
    src/core/IComponent.cpp
)

# UI files will be added as they are implemented
set(UI_SOURCES
    src/ui/DragDropWidget.cpp
    src/ui/MediaInfoWidget.cpp
    src/ui/MainWindow.cpp          # Task 3.1
    src/ui/PlaybackControls.cpp    # Task 3.2
    src/ui/LibraryWidget.cpp       # Task 5.4 - IMPLEMENTED
    src/ui/PlaylistWidget.cpp      # Task 5.4 - IMPLEMENTED
    src/ui/AudioEqualizerWidget.cpp # Task 6.1
    src/ui/AudioVisualizerWidget.cpp # Task 6.2
    src/ui/MiniPlayerWidget.cpp    # Task 7.1 - IMPLEMENTED
    src/ui/PictureInPictureWidget.cpp # Task 7.1 - IMPLEMENTED
    src/ui/VideoWidget.cpp         # Task 3.3 - IMPLEMENTED
    src/ui/AudioProcessorWidget.cpp
    src/ui/HotkeyManager.cpp
    src/ui/MediaKeysManager.cpp
    src/ui/NotificationManager.cpp
    src/ui/SubtitleControlWidget.cpp
    src/ui/SystemTrayManager.cpp
    # src/ui/LicenseViewer.cpp       # Task 12.2 - Missing file
    # src/ui/SettingsDialog.cpp      # Task 10.1
)

set(MEDIA_SOURCES
    src/media/VLCBackend.cpp
    src/media/PlaybackController.cpp
    src/media/MediaError.cpp
    src/media/HardwareAcceleration.cpp
    src/media/FileUrlSupport.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioEqualizer.cpp
    src/audio/AudioVisualizer.cpp
    src/audio/AudioProcessor.cpp
    src/audio/AudioOutputManager.cpp
    src/audio/AdvancedAudioProcessor.cpp
)

set(VIDEO_SOURCES
    src/video/VideoProcessor.cpp   # Task 7.1 - IMPLEMENTED
    src/video/VideoExporter.cpp    # Task 7.2 - IMPLEMENTED
)

# Add VLC stub for Windows builds
if(WIN32)
    list(APPEND MEDIA_SOURCES src/media/vlc_stub_windows.cpp)
    message(STATUS "Using VLC stub for Windows build")
endif()

set(DATA_SOURCES
    src/data/SettingsManager.cpp
    src/data/UserPreferences.cpp
    src/data/DatabaseManager.cpp      # Task 5.1
    src/data/MediaFile.cpp            # Task 5.1
    src/data/Playlist.cpp             # Task 5.1
    src/data/BackupManager.cpp        # Task 5.1 & 12.2 - IMPLEMENTED
    src/data/LibraryManager.cpp       # Task 5.2
    src/data/MediaScanner.cpp         # Task 5.2
    src/data/MetadataExtractor.cpp    # Task 5.2
    src/data/PlaylistManager.cpp      # Task 5.3
)

set(NETWORK_SOURCES
    src/network/VideoCastingManager.cpp # Task 7.3 - IMPLEMENTED
    src/network/NetworkStreamManager.cpp # Task 8.1 - IMPLEMENTED
    src/network/InternetStreamingService.cpp # Task 8.2 - IMPLEMENTED
    src/network/NetworkDiscoveryManager.cpp # Task 8.3 - IMPLEMENTED
    # Additional network files will be added as implemented:
    # src/network/NetworkProtocols.cpp    # Task 8.1
)

set(AI_SOURCES
    src/ai/AISubtitleGenerator.cpp    # Task 4.3
    src/ai/SubtitleLanguageDetector.cpp # Task 4.3
    src/ai/SubtitleTranslator.cpp     # Task 4.3
    src/ai/AISubtitleManager.cpp      # Task 4.3
    # Additional AI files will be added as implemented:
    # src/ai/AIContentAnalysis.cpp     # Task 9.1
    # src/ai/AIRecommendations.cpp     # Task 9.2
)

set(SUBTITLE_SOURCES
    src/subtitles/SubtitleManager.cpp
    src/subtitles/SubtitleRenderer.cpp
)

set(SECURITY_SOURCES
    src/security/SecurityManager.cpp      # Task 12.1 - IMPLEMENTED
    src/security/ParentalControlManager.cpp # Task 12.2 - IMPLEMENTED
)

set(STABILITY_SOURCES
    src/stability/CrashReporter.cpp   # Task 12.2 - IMPLEMENTED
    # src/stability/AutoUpdater.cpp     # Task 12.2 - PARTIAL
    # src/stability/SafeModeManager.cpp # Task 12.2
)

set(PLATFORM_SOURCES
    src/platform/InstallerManager.cpp # Task 13.3 - IMPLEMENTED
)

# Add platform-specific sources
if(WIN32)
    list(APPEND PLATFORM_SOURCES src/platform/WindowsPlatform.cpp)  # Task 13.1 - IMPLEMENTED
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/LinuxPlatform.cpp)    # Task 13.2 - IMPLEMENTED
endif()

# Header files for moc processing
set(HEADER_FILES
    include/media/IMediaEngine.h
    include/media/VLCBackend.h
    include/ui/MainWindow.h
    include/ui/PlaybackControls.h
    include/ui/MediaInfoWidget.h
    include/ui/DragDropWidget.h
    include/ui/LibraryWidget.h
    include/ui/PlaylistWidget.h
    include/ui/AudioEqualizerWidget.h
    include/ui/AudioVisualizerWidget.h
    include/ui/MiniPlayerWidget.h
    include/ui/PictureInPictureWidget.h
    include/ui/VideoWidget.h
    include/ui/AudioProcessorWidget.h
    include/ui/HotkeyManager.h
    include/ui/MediaKeysManager.h
    include/ui/NotificationManager.h
    include/ui/SubtitleControlWidget.h
    include/ui/SystemTrayManager.h
    include/audio/AudioEqualizer.h
    include/audio/AudioVisualizer.h
    include/audio/AudioProcessor.h
    include/audio/AudioOutputManager.h
    include/audio/AdvancedAudioProcessor.h
    include/video/VideoProcessor.h
    include/video/VideoExporter.h
    include/network/VideoCastingManager.h
    include/network/NetworkStreamManager.h
    include/network/InternetStreamingService.h
    include/network/NetworkDiscoveryManager.h
    include/data/DatabaseManager.h
    include/data/MediaFile.h
    include/data/Playlist.h
    include/data/BackupManager.h
    include/data/LibraryManager.h
    include/data/MediaScanner.h
    include/data/MetadataExtractor.h
    include/data/PlaylistManager.h
    include/ai/AISubtitleGenerator.h
    include/ai/SubtitleLanguageDetector.h
    include/ai/SubtitleTranslator.h
    include/ai/AISubtitleManager.h
    include/subtitles/SubtitleManager.h
    include/subtitles/SubtitleRenderer.h
    include/security/SecurityManager.h
    include/security/ParentalControlManager.h
    include/stability/CrashReporter.h
    include/stability/AutoUpdater.h
    include/platform/InstallerManager.h
    include/EonPlayApplication.h
    include/ComponentManager.h
    include/EventBus.h
)

# Add platform-specific headers
if(WIN32)
    list(APPEND HEADERS include/platform/WindowsPlatform.h)
elseif(UNIX AND NOT APPLE)
    list(APPEND HEADERS include/platform/LinuxPlatform.h)
endif()

# Main executable
add_executable(EonPlay
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${MEDIA_SOURCES}
    ${AUDIO_SOURCES}
    ${VIDEO_SOURCES}
    ${DATA_SOURCES}
    ${NETWORK_SOURCES}
    ${AI_SOURCES}
    ${SUBTITLE_SOURCES}
    ${SECURITY_SOURCES}
    ${STABILITY_SOURCES}
    ${PLATFORM_SOURCES}
)

# Set target include directories
target_include_directories(EonPlay PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${LIBVLC_INCLUDE_DIRS}
)

# Set target properties for MOC processing
set_target_properties(EonPlay PROPERTIES
    AUTOMOC_MACRO_NAMES "Q_OBJECT;Q_GADGET;Q_NAMESPACE"
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include"
)

# Link libraries
target_link_libraries(EonPlay
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Sql
)

# Link WebSockets if available
if(TARGET Qt6::WebSockets)
    target_link_libraries(EonPlay Qt6::WebSockets)
    target_compile_definitions(EonPlay PRIVATE HAVE_QT_WEBSOCKETS)
endif()

# Link DBus only on non-Windows platforms
if(NOT WIN32 AND TARGET Qt6::DBus)
    target_link_libraries(EonPlay Qt6::DBus)
endif()

# Platform-specific linking
if(WIN32)
    # Link VLC libraries if available, otherwise use stubs
    if(LIBVLC_LIBRARY AND LIBVLCCORE_LIBRARY)
        target_link_libraries(EonPlay
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
        )
    endif()
    
    # Windows-specific libraries
    target_link_libraries(EonPlay
        winmm
        ole32
        oleaut32
        uuid
        d3d9
        dxva2
    )
else()
    target_link_libraries(EonPlay
        ${LIBVLC_LIBRARIES}
    )
    
    # Linux-specific libraries
    target_link_libraries(EonPlay
        pthread
        dl
    )
    
    # Hardware acceleration libraries for Linux
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBVA libva libva-x11)
        pkg_check_modules(LIBVDPAU vdpau)
        
        if(LIBVA_FOUND)
            target_link_libraries(EonPlay ${LIBVA_LIBRARIES})
            target_include_directories(EonPlay PRIVATE ${LIBVA_INCLUDE_DIRS})
            target_compile_definitions(EonPlay PRIVATE HAVE_LIBVA)
        endif()
        
        if(LIBVDPAU_FOUND)
            target_link_libraries(EonPlay ${LIBVDPAU_LIBRARIES})
            target_include_directories(EonPlay PRIVATE ${LIBVDPAU_INCLUDE_DIRS})
            target_compile_definitions(EonPlay PRIVATE HAVE_LIBVDPAU)
        endif()
    endif()
    
    # X11 libraries for hardware acceleration
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(EonPlay ${X11_LIBRARIES})
        target_include_directories(EonPlay PRIVATE ${X11_INCLUDE_DIR})
    endif()
endif()

# Additional MOC configuration
set(CMAKE_AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include")
set(CMAKE_AUTOMOC_PATH_PREFIX ON)

# Resources (optional)
set(RESOURCE_FILES
    resources/icons/play.png
    resources/icons/pause.png
    resources/icons/stop.png
    resources/icons/app_icon.png
    resources/themes/dark.qss
    resources/themes/light.qss
)

# Check if all resource files exist
set(ALL_RESOURCES_EXIST TRUE)
foreach(RESOURCE_FILE ${RESOURCE_FILES})
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${RESOURCE_FILE}")
        set(ALL_RESOURCES_EXIST FALSE)
        message(STATUS "Resource file not found: ${RESOURCE_FILE}")
    endif()
endforeach()

if(ALL_RESOURCES_EXIST)
    qt6_add_resources(EonPlay "resources"
        PREFIX "/"
        FILES ${RESOURCE_FILES}
    )
    message(STATUS "Added embedded resources")
else()
    message(STATUS "Building without embedded resources - some resource files are missing")
endif()

# Testing configuration (optional)
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Plugin system
add_subdirectory(plugins)

# Examples (optional)
option(BUILD_EXAMPLES "Build example programs" OFF)
if(BUILD_EXAMPLES)
    # VLC Backend example
    add_executable(vlc_backend_example
        examples/vlc_backend_example.cpp
        ${CORE_SOURCES}
        ${MEDIA_SOURCES}
    )
    
    target_link_libraries(vlc_backend_example
        Qt6::Core
    )
    
    # PlaybackController example
    add_executable(playback_controller_example
        examples/playback_controller_example.cpp
        ${CORE_SOURCES}
        ${MEDIA_SOURCES}
    )
    
    target_include_directories(playback_controller_example PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(playback_controller_example
        Qt6::Core
    )
    
    # Hardware Acceleration example
    add_executable(hardware_acceleration_example
        examples/hardware_acceleration_example.cpp
        ${CORE_SOURCES}
        ${MEDIA_SOURCES}
        ${DATA_SOURCES}
    )
    
    target_include_directories(hardware_acceleration_example PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(hardware_acceleration_example
        Qt6::Core
    )
    
    # Advanced Playback example
    add_executable(advanced_playback_example
        examples/advanced_playback_example.cpp
        ${CORE_SOURCES}
        ${MEDIA_SOURCES}
        ${DATA_SOURCES}
    )
    
    target_include_directories(advanced_playback_example PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(advanced_playback_example
        Qt6::Core
    )
    
    # File URL Support example
    add_executable(file_url_support_example
        examples/file_url_support_example.cpp
        ${CORE_SOURCES}
        ${UI_SOURCES}
        ${MEDIA_SOURCES}
        ${DATA_SOURCES}
    )
    
    target_include_directories(file_url_support_example PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(file_url_support_example
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
    
    # Platform-specific linking for examples
    if(WIN32)
        target_link_libraries(vlc_backend_example
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
        )
        target_link_libraries(playback_controller_example
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
        )
        target_link_libraries(hardware_acceleration_example
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
            d3d9
            dxva2
        )
        target_link_libraries(advanced_playback_example
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
        )
        target_link_libraries(file_url_support_example
            ${LIBVLC_LIBRARY}
            ${LIBVLCCORE_LIBRARY}
        )
    else()
        target_link_libraries(vlc_backend_example
            ${LIBVLC_LIBRARIES}
        )
        target_link_libraries(playback_controller_example
            ${LIBVLC_LIBRARIES}
        )
        target_link_libraries(hardware_acceleration_example
            ${LIBVLC_LIBRARIES}
        )
        target_link_libraries(advanced_playback_example
            ${LIBVLC_LIBRARIES}
        )
        target_link_libraries(file_url_support_example
            ${LIBVLC_LIBRARIES}
        )
        
        # Hardware acceleration libraries for example
        if(PkgConfig_FOUND)
            if(LIBVA_FOUND)
                target_link_libraries(hardware_acceleration_example ${LIBVA_LIBRARIES})
                target_include_directories(hardware_acceleration_example PRIVATE ${LIBVA_INCLUDE_DIRS})
                target_compile_definitions(hardware_acceleration_example PRIVATE HAVE_LIBVA)
            endif()
            
            if(LIBVDPAU_FOUND)
                target_link_libraries(hardware_acceleration_example ${LIBVDPAU_LIBRARIES})
                target_include_directories(hardware_acceleration_example PRIVATE ${LIBVDPAU_INCLUDE_DIRS})
                target_compile_definitions(hardware_acceleration_example PRIVATE HAVE_LIBVDPAU)
            endif()
        endif()
        
        # X11 libraries for hardware acceleration example
        if(X11_FOUND)
            target_link_libraries(hardware_acceleration_example ${X11_LIBRARIES})
            target_include_directories(hardware_acceleration_example PRIVATE ${X11_INCLUDE_DIR})
        endif()
    endif()
endif()

# Installation configuration
if(WIN32)
    # Windows installation
    install(TARGETS EonPlay
        RUNTIME DESTINATION bin
    )
    
    # Install Qt libraries
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_WINDEPLOYQT_EXECUTABLE ${QT_QMAKE_EXECUTABLE} PATH)
    set(QT_WINDEPLOYQT_EXECUTABLE "${QT_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")
    
    install(CODE "
        execute_process(COMMAND ${QT_WINDEPLOYQT_EXECUTABLE} \${CMAKE_INSTALL_PREFIX}/bin/EonPlay.exe)
    ")
    
    # Code signing (if certificate available)
    if(DEFINED CODESIGN_CERTIFICATE)
        install(CODE "
            execute_process(COMMAND signtool sign /f ${CODESIGN_CERTIFICATE} /t http://timestamp.digicert.com \${CMAKE_INSTALL_PREFIX}/bin/EonPlay.exe)
        ")
    endif()
    
else()
    # Linux installation
    install(TARGETS EonPlay
        RUNTIME DESTINATION bin
    )
    
    # Install desktop file
    install(FILES resources/linux/eonplay.desktop
        DESTINATION share/applications
    )
    
    # Install icon
    install(FILES resources/icons/app_icon.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME eonplay.png
    )
endif()

# Packaging configuration
set(CPACK_PACKAGE_NAME "EonPlay")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Timeless, futuristic media player - play for eons")
set(CPACK_PACKAGE_VENDOR "EonPlay Team")

if(WIN32)
    # Windows packaging
    set(CPACK_GENERATOR "WIX;NSIS")
    set(CPACK_WIX_UPGRADE_GUID "12345678-1234-1234-1234-123456789012")
    set(CPACK_NSIS_DISPLAY_NAME "EonPlay - Timeless Media Player")
    set(CPACK_NSIS_PACKAGE_NAME "EonPlay")
    set(CPACK_NSIS_CONTACT "support@mediaplayer.com")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://mediaplayer.com")
else()
    # Linux packaging
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "EonPlay Team <support@eonplay.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libqt6multimedia6, libvlc5")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtmultimedia, vlc-devel")
endif()

include(CPack)