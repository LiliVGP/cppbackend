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
    apt install -y ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

RUN mkdir -p /app/data && chown -R www:www /app

# Копируем собранный сервер
COPY --from=build /app/build/game_server /app/

# Копируем статику и данные
COPY ./data /app/data
COPY ./static /app/static

# Создаём виртуальное окружение в /tmp (проблем с правами не будет)
RUN python3 -m venv /tmp/venv && \
    /tmp/venv/bin/pip install pytest pytest-quickcheck pytest-xprocess pytest-parallel requests

# Создаём директорию, которую ищет скрипт run.sh
RUN mkdir -p /home/runner/work/cppbackend/cppbackend/cpp-backend-tests-practicum/scripts/sprint3

# Создаём символическую ссылку: тесты будут искать /home/runner/..., а реально использовать /tmp/venv
RUN ln -s /tmp/venv /home/runner/work/cppbackend/cppbackend/cpp-backend-tests-practicum/scripts/sprint3/.venv

USER www
WORKDIR /app

ENV STATIC_ROOT=/app/static

# Запускаем сервер с правильными флагами
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]