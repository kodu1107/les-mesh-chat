cmake_minimum_required(VERSION 3.20)

project(
    les_mesh_chat
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)

add_executable(
    les-chatd
    src/main.cpp
    src/application.cpp
)

target_include_directories(
    les-chatd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    les-chatd
    PRIVATE
        PkgConfig::LIBEVENT
)

target_compile_options(
    les-chatd
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
)

install(
    TARGETS les-chatd
    RUNTIME DESTINATION sbin
)

enable_testing()

add_test(
    NAME les-chatd-help
    COMMAND les-chatd --help
)