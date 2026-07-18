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

# Устанавливаем Python и всё необходимое для работы тестов
RUN apt update && \
    apt install -y \
      python3 \
      python3-venv \
      python3-pip \
      ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Создаём пользователя www
RUN groupadd -r www && useradd -r -g www www

# Создаём директории для приложения и тестового окружения
RUN mkdir -p /app/data \
             /app/static \
             /home/runner/work/cppbackend/cppbackend/cpp-backend-tests-practicum/scripts/sprint3

# Копируем собранный сервер
COPY --from=build /app/build/game_server /app/

# Копируем статику и данные
COPY ./data /app/data
COPY ./static /app/static

# Создаём виртуальное окружение в /home/runner/work/... (как ожидает скрипт)
# Здесь мы работаем от root, чтобы создать директорию и установить пакеты
RUN python3 -m venv /home/runner/work/cppbackend/cppbackend/cpp-backend-tests-practicum/scripts/sprint3/.venv && \
    /home/runner/work/cppbackend/cppbackend/cpp-backend-tests-practicum/scripts/sprint3/.venv/bin/pip install pytest pytest-quickcheck pytest-xprocess pytest-parallel requests

# Передаём права на всю директорию /home/runner/... пользователю www
# Теперь пользователь www сможет активировать окружение (там есть файл activate)
RUN chown -R www:www /home/runner/work/cppbackend/

# Переключаемся на пользователя www
USER www
WORKDIR /app

ENV STATIC_ROOT=/app/static

# Запускаем сервер с правильными флагами
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]