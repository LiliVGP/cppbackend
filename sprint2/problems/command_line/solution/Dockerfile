FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      git && \
    pip3 install conan==1.52.0

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN apt update && \
    apt install -y \
     ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

RUN mkdir -p /app/data && \
    chown -R www:www /app

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

USER www
WORKDIR /app

ENV STATIC_ROOT=/app/static

ENTRYPOINT ["/app/game_server", "/app/data/config.json"]
