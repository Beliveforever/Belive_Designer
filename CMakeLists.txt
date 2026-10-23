cmake_minimum_required(VERSION 3.24)

project(BeliveDesigner LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(APPLE)
    set(BKLV_HOST "macos")
elseif(WIN32)
    set(BKLV_HOST "windows")
else()
    set(BKLV_HOST "linux")
endif()

set(BKLV_GRAPHICS "gl" CACHE STRING "Graphics backend: gl or vk")
set_property(CACHE BKLV_GRAPHICS PROPERTY STRINGS gl vk)

set(BKLV_OUT_DIR "${CMAKE_SOURCE_DIR}/dist")
add_subdirectory(BekUI/bklv)

add_executable(designer
    src/main.cpp
    src/core/model.cpp
    src/core/document.cpp
    src/ui/ui.cpp
    src/ui/preview.cpp
    src/ui/panels.cpp
    src/ui/app.cpp
)

target_include_directories(designer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(designer PRIVATE bklv::bklv)
target_compile_definitions(designer PRIVATE BKLV_DESIGNER_VERSION="1.0.0")

set_target_properties(designer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
)

# Runtime resources: bklv theme/fonts + the designer sample project.
add_custom_command(TARGET designer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/BekUI/resources"
        "${BKLV_OUT_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/projects"
        "${BKLV_OUT_DIR}/projects"
    COMMENT "Sync resources and projects into dist/"
    VERBATIM
)
