cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# --- Статическая библиотека с игровой моделью ---
# Включает только логику игры и её прямые зависимости.
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
target_link_libraries(game_model PUBLIC CONAN_PKG::boost)

# --- Основной исполняемый файл сервера ---
add_executable(game_server
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/request_handler.h
    src/request_handler.cpp
    src/sdk.h
)

target_link_libraries(game_server PRIVATE game_model Threads::Threads)

# --- Исполняемый файл тестов ---
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
)

target_link_libraries(game_server_tests PRIVATE game_model CONAN_PKG::catch2 Threads::Threads)

# --- Интеграция с CTest ---
include(CTest)
include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
