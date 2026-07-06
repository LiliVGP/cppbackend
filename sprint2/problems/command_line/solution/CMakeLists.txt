cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключаем сгенерированный Conan файл
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Указываем FindBoost.cmake, что нужно искать статические библиотеки
set(Boost_USE_STATIC_LIBS ON)

# Явно передаём пути к Boost, которые предоставил Conan
set(Boost_INCLUDE_DIR ${CONAN_INCLUDE_DIRS})
set(Boost_LIBRARY_DIR ${CONAN_LIB_DIRS})

# Находим Boost через стандартный модуль CMake
find_package(Boost REQUIRED COMPONENTS program_options)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/api_handler.h
    src/player.h
    src/authorization.h
    src/ticker.h
)

# Линкуем с найденным Boost и Threads
target_link_libraries(game_server PRIVATE Boost::program_options Threads::Threads)