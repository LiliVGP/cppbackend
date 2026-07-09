cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
	main.cpp
	http_server.cpp
	http_server.h
	sdk.h
	model.h
	model.cpp
	tagged.h
	boost_json.cpp
	json_loader.h
	json_loader.cpp
	request_handler.cpp
	request_handler.h
)
target_link_libraries(game_server PRIVATE Threads::Threads ${Boost_LIBRARIES})
