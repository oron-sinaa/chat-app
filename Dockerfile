FROM ubuntu:22.04

RUN apt update

RUN apt install -yqq \
    nlohmann-json3-dev \
    build-essential \
    cmake \
    zlib1g-dev

WORKDIR /workspace

COPY . .

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

ENTRYPOINT ["./workspace/build/chat_app"]
