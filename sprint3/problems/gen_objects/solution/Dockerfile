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

# Устанавливаем зависимости
RUN conan profile new default --detect && \
    conan install . --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Собираем в корне (так conanbuildinfo_multi.cmake будет найден)
RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build .

# ---- СТАДИЯ ЗАПУСКА ----
FROM ubuntu:22.04 AS run

# Устанавливаем библиотеки для запуска
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libstdc++6 \
      ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Создаём пользователя и папку
RUN mkdir -p /app && \
    useradd -m -u 1000 www && \
    chown -R www:www /app

USER www
WORKDIR /app

# Копируем бинарник и данные
COPY --from=build /app/game_server /app/game_server
COPY --from=build /app/data /app/data
COPY --from=build /app/static /app/static

# ВАЖНО: Проверяем, что бинарник существует и библиотеки корректны
RUN test -f /app/game_server && echo "✅ game_server exists" || (echo "❌ game_server missing" && exit 1)
RUN ldd /app/game_server

ENV STATIC_ROOT=/app/static

# Явно указываем порт и слушаем все интерфейсы
EXPOSE 8080

# Запускаем сервер с явным указанием порта (если сервер поддерживает --port)
CMD sleep 2 && /app/game_server --config-file /app/data/config.json --www-root /app/static --port 8080
