FROM gcc:11.3 as build

RUN apt update && \
    apt install -y --no-install-recommends \
      cmake \
      make \
      python3 \
      python3-pip \
    && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
COPY ./src /app/src
COPY CMakeLists.txt /app/
COPY ./tests /app/tests

RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY ./static /app/static
RUN chmod -R a+r /app/data /app/static

EXPOSE 8080
WORKDIR /app
ENTRYPOINT ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static/"]
