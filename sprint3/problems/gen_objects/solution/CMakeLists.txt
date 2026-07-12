cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

add_executable(game_server
    src/main.cpp
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/geom.h
    src/tagged.h
    src/json_loader.h
    src/json_loader.cpp
    src/game_server.h
    src/game_server.cpp
)
target_link_libraries(game_server PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads) 

add_executable(game_server_tests
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/geom.h
    src/tagged.h
    tests/loot_generator_tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
