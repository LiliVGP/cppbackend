cmake_minimum_required(VERSION 3.11)

project(collision_detection CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключаем конфигурацию от Conan для мульти-конфигураций
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Библиотека с логикой обнаружения столкновений
add_library(collision_detection_lib STATIC
    src/collision_detector.h
    src/collision_detector.cpp
)

# Явно указываем, что библиотека публично включает папку src
target_include_directories(collision_detection_lib PUBLIC src)
target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

# Тестовая программа
add_executable(collision_detection_tests
    tests/collision-detector-tests.cpp
)

# --- ЯВНО ДОБАВЛЯЕМ ПУТИ К ЗАГОЛОВКАМ CATCH2 ---
# Для cmake_multi Conan генерирует переменные для каждой конфигурации.
# Мы берём пути для Release (так как мы собираем Release).
# Также добавляем путь к корню, чтобы компилятор видел папку catch2/ внутри include.

# 1. Добавляем путь к include-директориям Catch2 из переменной для Release
target_include_directories(collision_detection_tests PRIVATE ${CONAN_INCLUDE_DIRS_CATCH2_RELEASE})

# 2. Добавляем путь к корню include (на случай, если он отличается)
# Иногда Conan генерирует путь к корню include как CONAN_INCLUDE_DIRS_CATCH2, 
# но для надёжности добавляем оба варианта.
if(DEFINED CONAN_INCLUDE_DIRS_CATCH2)
    target_include_directories(collision_detection_tests PRIVATE ${CONAN_INCLUDE_DIRS_CATCH2})
endif()

# Линкуем тесты
target_link_libraries(collision_detection_tests CONAN_PKG::catch2 collision_detection_lib)