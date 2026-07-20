# Не просто создаём образ, но даём ему имя build
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.54.0

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    # 1. Обновляем рецепты (чтобы избежать ошибки libbacktrace) и устанавливаем обе конфигурации
    conan update --all && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Debug && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    # 2. Генератор cmake_multi подразумевает, что вы указываете build_type в CMake, но не передаёте -DCMAKE_BUILD_TYPE
    # Для cmake_multi просто запускаем cmake без указания build_type
    cmake .. && \
    cmake --build . --config Release && \
    ls

ENTRYPOINT ["/app/build/Release/collision_detection_tests"]
