FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      git && \
    pip3 install conan==1.64.1

COPY conanfile.txt /app/
COPY CMakeLists.txt /app/
COPY ./src /app/src
COPY ./tests /app/tests

RUN mkdir /app/build && cd /app/build && \
    conan profile new default --detect && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN apt update && \
    apt install -y \
      python3 \
      python3-venv \
      python3-pip \
      ca-certificates && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/game_server /app/

COPY ./data /app/data
COPY ./static /app/static

WORKDIR /app

ENV STATIC_ROOT=/app/static

# Запускаем сервер с правильными флагами
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]
