cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# 1. Подключаем информацию от Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 2. Создаем статическую библиотеку с игровой моделью
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

# Указываем include-директории и линкуем Boost для библиотеки
target_include_directories(game_model PUBLIC src)
target_link_libraries(game_model PUBLIC CONAN_PKG::boost)

# 3. Основная программа (сервер)
add_executable(game_server
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/request_handler.h
    src/request_handler.cpp
    src/sdk.h
)

target_link_libraries(game_server PRIVATE game_model Threads::Threads)

# 4. Тесты
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
)

target_link_libraries(game_server_tests PRIVATE game_model CONAN_PKG::catch2 Threads::Threads)

# 5. Интеграция с CTest
include(CTest)

# Находим Catch.cmake (универсальный способ для обычного генератора cmake)
if(EXISTS ${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
    include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
elseif(EXISTS ${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
    include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
else()
    message(WARNING "Файл Catch.cmake не найден. Тесты CTest не будут автоматически обнаружены.")
endif()

catch_discover_tests(game_server_tests)
