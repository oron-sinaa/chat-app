FROM ubuntu:22.04 AS helper

RUN apt update

RUN apt install -yqq \
    nlohmann-json3-dev \
    build-essential \
    cmake \
    zlib1g-dev \
    libssl-dev \
    git

WORKDIR /workspace

COPY . .

RUN git submodule add https://github.com/uNetworking/uWebSockets.git uWebSockets \
    && cd uWebSockets \
    && git submodule update --init uSockets

RUN cd /workspace/uWebSockets/uSockets \
    && make \
    && cp /workspace/uWebSockets/uSockets/uSockets.a /usr/local/lib/ \
    && cp /workspace/uWebSockets/uSockets/src/libusockets.h /usr/local/include/ \
    && cp -r /workspace/uWebSockets/uSockets/src/eventing /usr/local/include/ \
    && cp -r /workspace/uWebSockets/uSockets/src/crypto /usr/local/include/ \
    && cp -r /workspace/uWebSockets/uSockets/src/io_uring /usr/local/include/ \
    && cd /workspace/uWebSockets \
    && make \
    && make install

RUN cd /workspace \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make

FROM ubuntu:22.04 AS release

COPY --from=helper /workspace/build/chat_app /workspace/build/chat_app

ENTRYPOINT ["/workspace/build/chat_app"]
