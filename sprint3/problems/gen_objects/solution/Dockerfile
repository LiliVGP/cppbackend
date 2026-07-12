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

# Создаём директории и устанавливаем права
RUN mkdir -p /app/data /app/static && chown -R www:www /app

# Копируем исполняемый файл (убедитесь, что путь правильный)
COPY --from=build /app/build/bin/game_server /app/game_server
RUN chmod +x /app/game_server

# Копируем данные и статику
COPY ./data /app/data
COPY ./static /app/static

USER www
WORKDIR /app

ENV STATIC_ROOT=/app/static

EXPOSE 8080

ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]
