cmake_minimum_required(VERSION 3.24)

project(modern_gl_viewer VERSION 0.2.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

option(MGV_BUILD_GLFW_APP "Build the GLFW viewer entrypoint" ON)
option(MGV_BUILD_QT_APP "Build the Qt 6 viewer entrypoint" OFF)
option(MGV_BUILD_QML_APP "Build the Qt Quick/QML main-menu entrypoint" OFF)
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

# Ozz remains an implementation detail of mgv::animation. Runtime applications
# do not need its import tools, samples, or test dependencies.
set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_fbx OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf OFF CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_postfix OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ozz
    GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
    GIT_TAG 6cbdc790123aa4731d82e255df187b3a8a808256 # 0.16.0
)
FetchContent_MakeAvailable(ozz)

add_library(mgv_core
    src/bounds.cpp
    src/camera.cpp
    src/camera_controller.cpp
    src/image_loader.cpp
    src/material.cpp
    src/obj_loader.cpp
    src/renderer.cpp
    src/runtime_paths.cpp
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

add_library(mgv_physics
    src/physics/collider.cpp
    src/physics/collision_detector.cpp
    src/physics/physics_world.cpp
    src/physics/rigid_body.cpp
    src/physics/units.cpp
)
add_library(mgv::physics ALIAS mgv_physics)
target_compile_features(mgv_physics PUBLIC cxx_std_20)
target_include_directories(mgv_physics PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mgv_physics PRIVATE mgv::project_warnings)

find_package(Threads REQUIRED)

add_library(mgv_network
    src/network/datagram.cpp
    src/network/network_service.cpp
    src/network/udp_transport.cpp
)
add_library(mgv::network ALIAS mgv_network)
target_compile_features(mgv_network PUBLIC cxx_std_20)
target_include_directories(mgv_network PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mgv_network
    PRIVATE mgv::project_warnings Threads::Threads
)
if(WIN32)
    target_link_libraries(mgv_network PRIVATE ws2_32)
endif()

add_library(mgv_hardware
    src/hardware/gspro_json_parser.cpp
    src/hardware/mlm2_pro_adapter.cpp
    src/hardware/vertex_adapter.cpp
)
add_library(mgv::hardware ALIAS mgv_hardware)
target_compile_features(mgv_hardware PUBLIC cxx_std_20)
target_include_directories(mgv_hardware PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mgv_hardware
    PUBLIC mgv::core
    PRIVATE mgv::project_warnings mgv::network
)

add_library(mgv_animation
    src/animation/animation_system.cpp
)
add_library(mgv::animation ALIAS mgv_animation)
target_compile_features(mgv_animation PUBLIC cxx_std_20)
target_include_directories(mgv_animation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mgv_animation
    PUBLIC mgv::core
    PRIVATE mgv::project_warnings ozz_animation
)

add_library(mgv_runtime
    src/engine.cpp
)
add_library(mgv::runtime ALIAS mgv_runtime)
target_compile_features(mgv_runtime PUBLIC cxx_std_20)
target_include_directories(mgv_runtime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(mgv_runtime
    PUBLIC mgv::animation mgv::core mgv::network mgv::physics mgv::hardware
    PRIVATE mgv::project_warnings
)

if(MGV_BUILD_QT_APP OR MGV_BUILD_QML_APP)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGLWidgets)
endif()

if(MGV_BUILD_QML_APP)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Qml Quick QuickControls2)
endif()

if(MGV_BUILD_GLFW_APP OR MGV_BUILD_QT_APP OR MGV_BUILD_QML_APP)
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
        PUBLIC mgv::runtime
        PRIVATE mgv::opengl glfw mgv::project_warnings
    )

    add_executable(mgv src/main.cpp)
    target_compile_features(mgv PRIVATE cxx_std_20)
    target_link_libraries(mgv PRIVATE mgv::glfw_app mgv::project_warnings)
    target_compile_definitions(mgv PRIVATE
        MGV_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
endif()

if(MGV_BUILD_QT_APP OR MGV_BUILD_QML_APP)
    add_library(mgv_qt_frontend
        src/qt/main_menu_controller.cpp
        src/qt/main_menu_controller.hpp
        src/qt/qt_engine_launcher.cpp
        src/qt/qt_engine_launcher.hpp
        src/qt/qt_viewer_widget.cpp
        src/qt/qt_viewer_widget.hpp
        src/qt/qt_viewer_window.cpp
        src/qt/qt_viewer_window.hpp
    )
    add_library(mgv::qt_frontend ALIAS mgv_qt_frontend)
    set_target_properties(mgv_qt_frontend PROPERTIES AUTOMOC ON)
    target_compile_features(mgv_qt_frontend PUBLIC cxx_std_20)
    target_include_directories(mgv_qt_frontend PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/qt>"
    )
    target_link_libraries(mgv_qt_frontend
        PUBLIC mgv::runtime Qt6::Core
        PRIVATE mgv::opengl mgv::project_warnings Qt6::Widgets Qt6::OpenGLWidgets
    )
endif()

if(MGV_BUILD_QT_APP)
    qt_add_executable(mgv_qt MANUAL_FINALIZATION
        src/qt/main.cpp
    )
    target_compile_features(mgv_qt PRIVATE cxx_std_20)
    target_link_libraries(mgv_qt PRIVATE
        mgv::qt_frontend
        mgv::project_warnings
        Qt6::Widgets
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

if(MGV_BUILD_QML_APP)
    set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)

    # Keep strict project warnings on our entrypoint without applying them to
    # the C++ cache sources generated by Qt's QML compiler.
    add_library(mgv_qml_entrypoint OBJECT src/qt/qml_main.cpp)
    target_compile_features(mgv_qml_entrypoint PRIVATE cxx_std_20)
    target_link_libraries(mgv_qml_entrypoint PRIVATE
        mgv::qt_frontend
        mgv::project_warnings
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
    )
    target_compile_definitions(mgv_qml_entrypoint PRIVATE
        MGV_DEFAULT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    qt_add_executable(mgv_qml MANUAL_FINALIZATION)
    set_target_properties(mgv_qml PROPERTIES AUTOMOC ON)
    qt_add_qml_module(mgv_qml
        URI MgvMenu
        VERSION 1.0
        QML_FILES qml/Main.qml
    )
    target_compile_features(mgv_qml PRIVATE cxx_std_20)
    target_link_libraries(mgv_qml PRIVATE
        mgv_qml_entrypoint
        mgv::qt_frontend
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
    )
    set_target_properties(mgv_qml PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
    )
    qt_finalize_executable(mgv_qml)
endif()

if(BUILD_TESTING)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG 2b60af89e23d28eefc081bc930831ee9d45ea58b # v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(mgv_tests
        tests/animation/animation_system_test.cpp
        tests/bounds_test.cpp
        tests/camera_test.cpp
        tests/camera_controller_test.cpp
        tests/engine_test.cpp
        tests/image_loader_test.cpp
        tests/obj_loader_test.cpp
        tests/network/datagram_test.cpp
        tests/network/network_service_test.cpp
        tests/network/udp_transport_test.cpp
        tests/hardware/launch_monitor_test.cpp
        tests/physics/collider_test.cpp
        tests/physics/collision_detector_test.cpp
        tests/physics/physics_world_test.cpp
        tests/physics/units_test.cpp
        tests/physics/aerodynamics_test.cpp
        tests/renderer_test.cpp
        tests/runtime_paths_test.cpp
        tests/scene_test.cpp
        tests/shader_loader_test.cpp
        tests/texture_test.cpp
        tests/transform_test.cpp
    )
    target_compile_features(mgv_tests PRIVATE cxx_std_20)
    target_link_libraries(mgv_tests PRIVATE
        mgv::runtime
        mgv::project_warnings
        ozz_animation_offline
        Catch2::Catch2WithMain
    )
    target_compile_definitions(mgv_tests PRIVATE
        MGV_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )

    if(TARGET mgv_qt_frontend)
        target_sources(mgv_tests PRIVATE tests/qt/main_menu_controller_test.cpp)
        target_link_libraries(mgv_tests PRIVATE mgv::qt_frontend)
    endif()

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(mgv_tests)
endif()

set(MGV_INSTALL_TARGETS)
if(TARGET mgv)
    list(APPEND MGV_INSTALL_TARGETS mgv)
endif()
if(TARGET mgv_qt)
    list(APPEND MGV_INSTALL_TARGETS mgv_qt)
endif()
if(TARGET mgv_qml)
    list(APPEND MGV_INSTALL_TARGETS mgv_qml)
endif()

if(MGV_INSTALL_TARGETS)
    install(TARGETS ${MGV_INSTALL_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
    )
    install(DIRECTORY assets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/modern-gl-viewer/assets
    )

    if(TARGET mgv_qml)
        if(Qt6_VERSION VERSION_LESS 6.5)
            # Qt 6.4 exposed this technology-preview output parameter under its
            # original name. Ubuntu 24.04 still ships that API.
            qt_generate_deploy_qml_app_script(
                TARGET mgv_qml
                FILENAME_VARIABLE mgv_qml_deploy_script
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
        else()
            qt_generate_deploy_qml_app_script(
                TARGET mgv_qml
                OUTPUT_SCRIPT mgv_qml_deploy_script
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
        endif()
        install(SCRIPT ${mgv_qml_deploy_script})
        set(CPACK_PACKAGE_EXECUTABLES "mgv_qml" "Modern GL Viewer")
    elseif(TARGET mgv_qt)
        set(CPACK_PACKAGE_EXECUTABLES "mgv_qt" "Modern GL Viewer")
    else()
        set(CPACK_PACKAGE_EXECUTABLES "mgv" "Modern GL Viewer")
    endif()

    set(CPACK_PACKAGE_NAME "modern-gl-viewer")
    set(CPACK_PACKAGE_VENDOR "Modern GL Viewer")
    set(CPACK_PACKAGE_CONTACT "Modern GL Viewer maintainers")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "A backend-neutral C++20 OBJ viewer with Qt Quick and OpenGL frontends"
    )
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Modern GL Viewer")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt"
        COPYONLY
    )
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")

    if(WIN32)
        set(CPACK_GENERATOR WIX)
        set(CPACK_WIX_UPGRADE_GUID "6F6814F2-38C2-4D55-8D55-5D81C09D5A2A")
    else()
        set(CPACK_GENERATOR DEB)
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        if(TARGET mgv_qml)
            set(CPACK_DEBIAN_PACKAGE_DEPENDS
                "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml-models, qml6-module-qtqml-workerscript"
            )
        endif()
    endif()

    include(CPack)
endif()
