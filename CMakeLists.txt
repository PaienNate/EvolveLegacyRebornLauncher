cmake_minimum_required(VERSION 3.25)
project(EvolveLegacyRebornLauncher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)

# BUILD WITH: cmake --build . --config Release
# we also require the latest release of N3N custom built as we make the keep_running bool a shared memory region that we can access to gracefully shut it down!
# make sure to apply the patch after N3N updates!
# Note: N3N needs to be built on linux/wsl with make and i can't be bothered to sink even more time into compiling it into a lib file that i can actually use myself

# Find the required Qt modules
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Multimedia MultimediaWidgets Network)

qt_standard_project_setup()

add_executable(${PROJECT_NAME} WIN32
        src/Launcher.cpp
        src/n3n/N3NHelper.cpp
        src/n3n/N3NHelper.h
        src/log/Logger.cpp
        src/log/Logger.h
        src/launcher/PipeHelper.cpp
        src/launcher/PipeHelper.h
        src/qt/DraggableLabel.cpp
        src/qt/DraggableLabel.h
        src/qt/FileDownloader.hpp
        src/qt/Updater.hpp
        src/GameFiles.h
        src/util/SaveFileHelper.cpp
        src/util/SaveFileHelper.h
)
target_include_directories(${PROJECT_NAME} PUBLIC "include/xxhash")
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Network)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "EvolveLauncher")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DPI_AWARE "PerMonitor")
target_compile_definitions(${PROJECT_NAME} PUBLIC LOGGER_FILENAME="Launcher.log")
#target_sources(${PROJECT_NAME} PRIVATE launcher.rc)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMENT "Copying Resources"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/console_usage.txt"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/console_usage.txt"
        COMMENT "Copying console usage guide"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/patch"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/patch"
        COMMENT "Copying game patch template"
)

add_executable(N3NManagerService
        src/N3NManager.cpp
        src/n3n/N3NHelper.cpp
        src/n3n/N3NHelper.h
        src/log/Logger.cpp
        src/log/Logger.h
)

target_link_libraries(N3NManagerService ws2_32)
set_target_properties(N3NManagerService PROPERTIES OUTPUT_NAME "EvolveN3NManager")
set_target_properties(N3NManagerService PROPERTIES LINK_FLAGS "/level='requireAdministrator' /uiAccess='false'")
target_compile_definitions(N3NManagerService PUBLIC LOGGER_FILENAME="N3NManager.log")

add_custom_command(TARGET N3NManagerService POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/"
        "$<TARGET_FILE_DIR:N3NManagerService>/bin"
        COMMENT "Copying bin"
)