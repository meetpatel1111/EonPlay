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
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets Network)

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
else()
    # Linux libVLC configuration
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVLC REQUIRED libvlc>=3.0.0)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${LIBVLC_INCLUDE_DIR}
)

# Source files
set(CORE_SOURCES
    src/core/EonPlayApplication.cpp
    src/core/ComponentManager.cpp
    src/core/EventBus.cpp
    src/core/IComponent.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/PlaybackControls.cpp
    src/ui/VideoWidget.cpp
    src/ui/PlaylistWidget.cpp
    src/ui/LibraryWidget.cpp
    src/ui/SettingsDialog.cpp
)

set(MEDIA_SOURCES
    src/media/VLCBackend.cpp
    src/media/PlaybackController.cpp
    src/media/MediaError.cpp
    src/media/HardwareAcceleration.cpp
)

set(DATA_SOURCES
    src/data/SettingsManager.cpp
    src/data/UserPreferences.cpp
    src/data/DatabaseManager.cpp
    src/data/LibraryManager.cpp
    src/data/MediaScanner.cpp
    src/data/MetadataExtractor.cpp
    src/data/PlaylistManager.cpp
)

set(NETWORK_SOURCES
    src/network/StreamingManager.cpp
    src/network/NetworkProtocols.cpp
)

set(AI_SOURCES
    src/ai/AIContentAnalysis.cpp
    src/ai/AIRecommendations.cpp
)

# Main executable
add_executable(EonPlay
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${MEDIA_SOURCES}
    ${DATA_SOURCES}
    ${NETWORK_SOURCES}
    ${AI_SOURCES}
)

# Link libraries
target_link_libraries(EonPlay
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(EonPlay
        ${LIBVLC_LIBRARY}
        ${LIBVLCCORE_LIBRARY}
    )
    
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

# Enable Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Resources
qt6_add_resources(EonPlay "resources"
    PREFIX "/"
    FILES
        resources/icons/play.png
        resources/icons/pause.png
        resources/icons/stop.png
        resources/icons/app_icon.png
        resources/themes/dark.qss
        resources/themes/light.qss
)

# Testing configuration
enable_testing()
add_subdirectory(tests)

# Plugin system
add_subdirectory(plugins)

# Examples (optional)
option(BUILD_EXAMPLES "Build example programs" ON)
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
    
    target_link_libraries(advanced_playback_example
        Qt6::Core
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