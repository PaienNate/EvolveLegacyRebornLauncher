cmake_minimum_required(VERSION 3.25)
project(EvolveLegacyRebornLauncher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BUILD WITH: cmake --build . --config Release
# we also require the latest release of N3N custom built as we make the keep_running bool a shared memory region that we can access to gracefully shut it down!
# make sure to apply the patch after N3N updates!
# Note: N3N needs to be built on linux/wsl with make and i can't be bothered to sink even more time into compiling it into a lib file that i can actually use myself

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/log/Logger.cpp
        src/log/Logger.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "EvolveLauncher")
target_compile_definitions(${PROJECT_NAME} PUBLIC LOGGER_FILENAME="Launcher.log")

add_executable(N3NManagerService
        src/n3nmanager.cpp
        src/n3n/N3NHelper.cpp
        src/n3n/N3NHelper.h
        src/log/Logger.cpp
        src/log/Logger.h
)

target_link_libraries(N3NManagerService ws2_32)
set_target_properties(N3NManagerService PROPERTIES OUTPUT_NAME "EvolveN3NManager")
target_compile_definitions(N3NManagerService PUBLIC LOGGER_FILENAME="N3NManager.log")

set(MANAGER_MANIFEST_FILE n3nmanager.manifest)

add_custom_command(TARGET N3NManagerService POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/${MANAGER_MANIFEST_FILE}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${MANAGER_MANIFEST_FILE})

if(WIN32)
    set_target_properties(N3NManagerService PROPERTIES
            MANIFEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${MANAGER_MANIFEST_FILE}"
    )
endif()