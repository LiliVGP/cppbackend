# ---- СТАДИЯ СБОРКИ ----
FROM gcc:11 AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      python3-pip \
      cmake \
      git && \
    pip3 install conan==1.64.1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY conanfile.txt .
COPY CMakeLists.txt .
COPY src ./src
COPY tests ./tests
COPY data ./data
COPY static ./static

# Conan: устанавливаем зависимости
RUN conan profile new default --detect && \
    conan install . --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# CMake: собираем прямо в корне /app
RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build .

# ---- СТАДИЯ ЗАПУСКА ----
FROM ubuntu:22.04 AS run

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libstdc++6 \
      ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /app && \
    useradd -m -u 1000 www && \
    chown -R www:www /app

USER www
WORKDIR /app

# Копируем бинарник (он лежит в /app, потому что собирали в корне)
COPY --from=build /app/game_server /app/game_server
COPY --from=build /app/data /app/data
COPY --from=build /app/static /app/static

ENV STATIC_ROOT=/app/static

ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]