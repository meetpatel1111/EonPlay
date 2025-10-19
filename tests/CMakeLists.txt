# Test configuration
find_package(Qt6 REQUIRED COMPONENTS Test)

# Test sources
set(TEST_SOURCES
    test_vlc_backend.cpp
    test_vlc_integration.cpp
    test_playback_controller.cpp
    test_playback_integration.cpp
    test_hardware_acceleration.cpp
)

# Core sources needed for tests
set(CORE_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ComponentManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/EventBus.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IComponent.cpp
    ${CMAKE_SOURCE_DIR}/src/media/VLCBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/media/PlaybackController.cpp
    ${CMAKE_SOURCE_DIR}/src/media/MediaError.cpp
    ${CMAKE_SOURCE_DIR}/src/media/HardwareAcceleration.cpp
)

# Create test executable
add_executable(MediaPlayerTests
    ${TEST_SOURCES}
    ${CORE_TEST_SOURCES}
)

# Include directories for tests
target_include_directories(MediaPlayerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${LIBVLC_INCLUDE_DIR}
)

# Link test libraries
target_link_libraries(MediaPlayerTests
    Qt6::Test
    Qt6::Core
    Qt6::Widgets
)

# Platform-specific linking for tests
if(WIN32)
    target_link_libraries(MediaPlayerTests
        ${LIBVLC_LIBRARY}
        ${LIBVLCCORE_LIBRARY}
        d3d9
        dxva2
    )
else()
    target_link_libraries(MediaPlayerTests
        ${LIBVLC_LIBRARIES}
    )
    
    # Hardware acceleration libraries for Linux tests
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBVA libva libva-x11)
        pkg_check_modules(LIBVDPAU vdpau)
        
        if(LIBVA_FOUND)
            target_link_libraries(MediaPlayerTests ${LIBVA_LIBRARIES})
            target_include_directories(MediaPlayerTests PRIVATE ${LIBVA_INCLUDE_DIRS})
            target_compile_definitions(MediaPlayerTests PRIVATE HAVE_LIBVA)
        endif()
        
        if(LIBVDPAU_FOUND)
            target_link_libraries(MediaPlayerTests ${LIBVDPAU_LIBRARIES})
            target_include_directories(MediaPlayerTests PRIVATE ${LIBVDPAU_INCLUDE_DIRS})
            target_compile_definitions(MediaPlayerTests PRIVATE HAVE_LIBVDPAU)
        endif()
    endif()
    
    # X11 libraries for hardware acceleration tests
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(MediaPlayerTests ${X11_LIBRARIES})
        target_include_directories(MediaPlayerTests PRIVATE ${X11_INCLUDE_DIR})
    endif()
endif()

# Add tests to CTest
add_test(NAME MediaPlayerTests COMMAND MediaPlayerTests)

# Enable code coverage if requested
if(ENABLE_COVERAGE)
    if(CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(MediaPlayerTests PRIVATE --coverage)
        target_link_libraries(MediaPlayerTests --coverage)
    endif()
endif()