cmake_minimum_required(VERSION 3.19)
project(qactor VERSION 0.1.0 LANGUAGES CXX)

# Windows 的默认前缀在 Program Files 下，安装需要管理员权限；未显式指定时
# 改为装进构建目录，让 cmake --install 开箱可用。
if(WIN32)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH
        "Install path prefix, prepended onto install directories" FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

add_library(qactor SHARED
    src/actor.h
    src/actor.cpp
    src/asynctask.h
    src/execthread.h
    src/execthread.cpp
    src/qactor_global.h
)
add_library(qactor::qactor ALIAS qactor)

target_compile_features(qactor PUBLIC cxx_std_17)
target_compile_definitions(qactor PRIVATE QACTOR_BUILD)
target_include_directories(qactor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(qactor PUBLIC Qt::Core)
set_target_properties(qactor PROPERTIES
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME qactor
)

option(QACTOR_BUILD_EXAMPLES "Build qactor example programs" ON)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(test_actor tests/test_actor.cpp)
    target_link_libraries(test_actor PRIVATE Qt::Core Qt::Test qactor::qactor)
    add_test(NAME actor_model COMMAND test_actor)
endif()

if(QACTOR_BUILD_EXAMPLES)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Network)

    qt_add_executable(qactor_counter examples/counter.cpp)
    target_link_libraries(qactor_counter PRIVATE Qt::Core qactor::qactor)

    qt_add_executable(qactor_tcp_echo examples/tcp_echo.cpp)
    target_link_libraries(qactor_tcp_echo PRIVATE Qt::Core Qt::Network qactor::qactor)
endif()

install(TARGETS qactor
    EXPORT qactorTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
    src/actor.h
    src/asynctask.h
    src/execthread.h
    src/qactor_global.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(qactor_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/qactor")
configure_package_config_file(
    cmake/qactorConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/qactorConfig.cmake"
    INSTALL_DESTINATION "${qactor_cmake_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/qactorConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT qactorTargets
    FILE qactorTargets.cmake
    NAMESPACE qactor::
    DESTINATION "${qactor_cmake_dir}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/qactorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/qactorConfigVersion.cmake"
    DESTINATION "${qactor_cmake_dir}"
)