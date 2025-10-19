# Plugin system configuration

# Plugin interface library
add_library(PluginInterface INTERFACE)
target_include_directories(PluginInterface INTERFACE
    ${CMAKE_SOURCE_DIR}/include/plugins
)

# Core plugins will be added in Phase 10 - Task 10.2
# TODO: Uncomment these when plugin directories are created:
# add_subdirectory(websocket_api)
# add_subdirectory(rest_api)  
# add_subdirectory(cli_interface)

# Plugin installation
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
    DESTINATION plugins
    FILES_MATCHING PATTERN "*.so" PATTERN "*.dll" PATTERN "*.dylib"
)