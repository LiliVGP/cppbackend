cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Явно находим нужные компоненты Boost
find_package(Boost COMPONENTS json system REQUIRED)

# --- Статическая библиотека ---
add_library(game_model STATIC
    src/model.h
    src/model.cpp
    src/tagged.h
    src/extra_data.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/boost_json.cpp
)

target_include_directories(game_model PUBLIC src)
# Линкуем конкретные компоненты, чтобы не тянуть лишнее (например, тестовый фреймворк)
target_link_libraries(game_model PUBLIC Boost::json Boost::system)

# --- Сервер ---
add_executable(game_server
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/request_handler.h
    src/request_handler.cpp
    src/sdk.h
)

target_link_libraries(game_server PRIVATE game_model Threads::Threads)

# --- Тесты ---
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
)

# Ключевое исправление: не линкуем Boost к тестам, чтобы не подтянуть boost_test
target_link_libraries(game_server_tests PRIVATE game_model CONAN_PKG::catch2 Threads::Threads)

# --- CTest ---
include(CTest)

if(DEFINED CONAN_BUILD_DIRS_CATCH2_DEBUG)
    include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
elseif(DEFINED CONAN_BUILD_DIRS_CATCH2)
    include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
endif()

catch_discover_tests(game_server_tests)
