cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(kinema)

set(SRC_FILES
    src/main.cpp
    src/Application.cpp
)
include_directories(include)
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Geni Engine
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/geni)
include_directories(external/geni/include)

# Find GLFW and GLEW for the main executable
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

include_directories(external/geni/thirdparty/stb)

# link engine library and dependencies
target_link_libraries(
    ${PROJECT_NAME}
    GeniEngine
    ${GLFW_LIBRARIES}
    ${GLEW_LIBRARIES}
)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
)