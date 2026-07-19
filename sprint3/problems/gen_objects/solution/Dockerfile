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

# Устанавливаем зависимости Conan
RUN conan profile new default --detect && \
    conan install . --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Собираем в корне (так conanbuildinfo_multi.cmake будет найден)
RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build .

# ---- СТАДИЯ ЗАПУСКА ----
FROM ubuntu:22.04 AS run

# Устанавливаем libstdc++ для запуска
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libstdc++6 \
      ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Создаём пользователя и папку (сначала mkdir, потом chown)
RUN mkdir -p /app && \
    useradd -m -u 1000 www && \
    chown -R www:www /app

USER www
WORKDIR /app

# Копируем бинарник и данные
COPY --from=build /app/game_server /app/game_server
COPY --from=build /app/data /app/data
COPY --from=build /app/static /app/static

# Убедимся, что статика доступна по пути, ожидаемому сервером
ENV STATIC_ROOT=/app/static

# ВАЖНО: Явно указываем порт 8080 и форсируем прослушивание всех интерфейсов
EXPOSE 8080

ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]