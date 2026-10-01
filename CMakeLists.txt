cmake_minimum_required(VERSION 3.24)

project(modern_gl_viewer VERSION 0.2.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

option(MGV_BUILD_GLFW_APP "Build the GLFW viewer entrypoint" ON)
option(MGV_BUILD_QT_APP "Build the Qt 6 viewer entrypoint" OFF)
option(MGV_HEADLESS_GLFW "Use GLFW's null platform for build/test environments" OFF)
option(MGV_GLFW_X11 "Build GLFW's X11 platform on Linux" ON)
option(MGV_GLFW_WAYLAND "Build GLFW's Wayland platform on Linux" OFF)
option(MGV_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

add_library(mgv_project_warnings INTERFACE)
add_library(mgv::project_warnings ALIAS mgv_project_warnings)
if(MSVC)
    target_compile_options(mgv_project_warnings INTERFACE /W4 /permissive-)
    if(MGV_WARNINGS_AS_ERRORS)
        target_compile_options(mgv_project_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(mgv_project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(MGV_WARNINGS_AS_ERRORS)
        target_compile_options(mgv_project_warnings INTERFACE -Werror)
    endif()
endif()

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_MakeAvailable(stb)

add_library(mgv_core
    src/camera.cpp
    src/camera_controller.cpp
    src/image_loader.cpp
    src/material.cpp
    src/obj_loader.cpp
    src/renderer.cpp
    src/scene.cpp
    src/shader_loader.cpp
    src/texture.cpp
    src/transform.cpp
)
add_library(mgv::core ALIAS mgv_core)
target_compile_features(mgv_core PUBLIC cxx_std_20)
target_include_directories(mgv_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_include_directories(mgv_core SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(mgv_core PRIVATE mgv::project_warnings)

if(MGV_BUILD_QT_APP)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGLWidgets)
endif()

if(MGV_BUILD_GLFW_APP OR MGV_BUILD_QT_APP)
    FetchContent_Declare(glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG 73db193f853e2ee079bf3ca8a64aa2eaf6459043 # v2.0.8
        SOURCE_SUBDIR cmake
    )
    FetchContent_MakeAvailable(glad)
    glad_add_library(mgv_glad REPRODUCIBLE API gl:core=4.1)

    add_library(mgv_opengl src/opengl/opengl_backend.cpp)
    add_library(mgv::opengl ALIAS mgv_opengl)
    target_compile_features(mgv_opengl PUBLIC cxx_std_20)
    target_include_directories(mgv_opengl PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    )
    target_link_libraries(mgv_opengl PUBLIC mgv::core PRIVATE mgv_glad mgv::project_warnings)
endif()

if(MGV_BUILD_GLFW_APP)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    if(MGV_HEADLESS_GLFW)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_NULL ON CACHE BOOL "" FORCE)
    else()
        set(GLFW_BUILD_X11 ${MGV_GLFW_X11} CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND ${MGV_GLFW_WAYLAND} CACHE BOOL "" FORCE)
        set(GLFW_BUILD_NULL OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # 3.4
    )
    FetchContent_MakeAvailable(glfw)

    add_library(mgv_glfw_app src/application.cpp)
    add_library(mgv::glfw_app ALIAS mgv_glfw_app)
    target_compile_features(mgv_glfw_app PUBLIC cxx_std_20)
    target_include_directories(mgv_glfw_app PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    )
    target_link_libraries(mgv_glfw_app
        PUBLIC mgv::core
        PRIVATE mgv::opengl glfw mgv::project_warnings
    )

    add_executable(mgv src/main.cpp)
    target_compile_features(mgv PRIVATE cxx_std_20)
    target_link_libraries(mgv PRIVATE mgv::glfw_app mgv::project_warnings)
    target_compile_definitions(mgv PRIVATE
        MGV_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
endif()

if(MGV_BUILD_QT_APP)
    qt_add_executable(mgv_qt MANUAL_FINALIZATION
        src/qt/main.cpp
        src/qt/qt_viewer_widget.cpp
        src/qt/qt_viewer_widget.hpp
    )
    target_compile_features(mgv_qt PRIVATE cxx_std_20)
    target_link_libraries(mgv_qt PRIVATE
        mgv::core
        mgv::opengl
        mgv::project_warnings
        Qt6::Widgets
        Qt6::OpenGLWidgets
    )
    target_compile_definitions(mgv_qt PRIVATE
        MGV_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
    set_target_properties(mgv_qt PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
    )
    qt_finalize_executable(mgv_qt)
endif()

if(BUILD_TESTING)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG 2b60af89e23d28eefc081bc930831ee9d45ea58b # v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(mgv_tests
        tests/camera_test.cpp
        tests/camera_controller_test.cpp
        tests/image_loader_test.cpp
        tests/obj_loader_test.cpp
        tests/renderer_test.cpp
        tests/scene_test.cpp
        tests/shader_loader_test.cpp
        tests/texture_test.cpp
        tests/transform_test.cpp
    )
    target_compile_features(mgv_tests PRIVATE cxx_std_20)
    target_link_libraries(mgv_tests PRIVATE
        mgv::core
        mgv::project_warnings
        Catch2::Catch2WithMain
    )
    target_compile_definitions(mgv_tests PRIVATE
        MGV_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(mgv_tests)
endif()
