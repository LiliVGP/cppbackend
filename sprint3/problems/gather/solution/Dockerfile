FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.54.0

COPY conanfile.txt /app/
# Важно: копируем пустой .conanignore, чтобы Conan не обновлял рецепты
COPY .conanignore /app/

RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Debug && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake .. && \
    cmake --build . --config Release && \
    ls

ENTRYPOINT ["/app/build/Release/collision_detection_tests"]
