cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(kinema)

set(KINEMA_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

# Geni Engine
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/geni)

# OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui objdetect calib3d)

# ImGui via FetchContent
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# GLFW
find_package(glfw3 CONFIG REQUIRED)

# GLEW
find_package(GLEW REQUIRED)

set(SRC_FILES
    src/main.cpp
    src/Application.cpp
    src/modules/MarkerDetection.cpp
    src/modules/HSVMarkerDetector.cpp
    src/modules/ArucoMarkerDetector.cpp
    src/modules/MotionRecorder.cpp
    src/modules/SkeletonDriver.cpp
    src/modules/VideoExporter.cpp
    src/modules/GlbExporter.cpp
    src/ui/UIManager.cpp
    ${IMGUI_SOURCES}
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src/
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    external/geni/thirdparty/stb
    external/geni/thirdparty/cgltf
    external/geni/include
    external/pfd
)

target_link_libraries(${PROJECT_NAME}
    GeniEngine
    glfw
    GLEW::GLEW
    ${OpenCV_LIBS}
)

# Copy kinema assets next to executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${KINEMA_ASSETS_DIR}
    ${CMAKE_BINARY_DIR}/assets
)
