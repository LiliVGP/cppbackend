FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
    python3-pip \
    cmake \
    && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug

COPY ./src /app/src
COPY ./data /app/data
COPY ./static /app/static
COPY CMakeLists.txt conanfile.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www

USER www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

RUN ls -la /app/static/
RUN ls -la /app/static/images/

ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static/"]
