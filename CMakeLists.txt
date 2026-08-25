cmake_minimum_required(VERSION 3.20)

project(
    les_mesh_chat
    VERSION 0.1.3
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(
    LESCHAT_WEB_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/web"
    CACHE PATH
    "Runtime path containing les-chatd web assets"
)

# Dependencies are installed in standard system paths.
# Avoid Ninja relinking during installagion.
set(CMAKE_SKIP_RPATH ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)

pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_executable(
    les-chatd
    src/main.cpp
    src/application.cpp
    src/json.cpp
    src/http.cpp
    src/message_codec.cpp
    src/message_service.cpp
    src/http_api.cpp
    src/peer_registry.cpp
    src/discovery_service.cpp
    src/message_store.cpp
    src/event_stream.cpp
    src/replication_service.cpp
    src/sync_service.cpp
)

target_include_directories(
    les-chatd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(
    les-chatd
    PRIVATE
        LESCHAT_WEB_ROOT="${LESCHAT_WEB_ROOT}"
)

target_link_libraries(
    les-chatd
    PRIVATE
        PkgConfig::LIBEVENT
        PkgConfig::JSONC
        PkgConfig::SQLITE3
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

add_executable(
    message-store-test
    tests/message_store_test.cpp
    src/message_store.cpp
)

target_include_directories(
    message-store-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    message-store-test
    PRIVATE
        PkgConfig::SQLITE3
)

target_compile_options(
    message-store-test
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
)

add_test(
    NAME message-store
    COMMAND message-store-test
)
