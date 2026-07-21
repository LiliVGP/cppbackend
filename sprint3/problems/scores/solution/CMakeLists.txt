cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.h
    src/request_handler.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/extra_data.h
    src/collision_detector.h
    src/collision_detector.cpp
    src/geom.h
)

target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server PRIVATE CONAN_PKG::boost)

add_executable(game_server_tests
    src/model.h
    src/model.cpp
    src/tagged.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/collision_detector.h
    src/collision_detector.cpp
    src/geom.h
    tests/loot_generator_tests.cpp
)

target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
