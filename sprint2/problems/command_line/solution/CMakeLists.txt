cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключаем сгенерированный Conan файл
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

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

# Добавляем пути к заголовочным файлам Boost
target_include_directories(game_server PRIVATE ${CONAN_INCLUDE_DIRS})

# Линкуем все библиотеки, которые Conan нашёл (включая boost_program_options)
target_link_libraries(game_server PRIVATE ${CONAN_LIBS} Threads::Threads)
