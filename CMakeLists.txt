cmake_minimum_required(VERSION 3.19)
project(qactor VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

qt_add_library(qactor STATIC
    src/actor.h
    src/asynctask.h
    src/threadpool.h
    src/threadpool.cpp
)
add_library(qactor::qactor ALIAS qactor)

target_compile_features(qactor PUBLIC cxx_std_17)
target_include_directories(qactor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(qactor PUBLIC Qt::Core)

qt_add_executable(qactor_counter examples/counter.cpp)
target_link_libraries(qactor_counter PRIVATE Qt::Core qactor::qactor)
# qt_add_executable 默认按 GUI 程序创建 Windows 目标，这两个是控制台程序
set_target_properties(qactor_counter PROPERTIES WIN32_EXECUTABLE OFF)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(test_actor tests/test_actor.cpp)
    target_link_libraries(test_actor PRIVATE Qt::Core Qt::Test qactor::qactor)
    set_target_properties(test_actor PROPERTIES WIN32_EXECUTABLE OFF)

    add_test(NAME actor_model COMMAND test_actor)
endif()
