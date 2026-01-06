FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libopencv-core4.5d \
    libopencv-imgcodecs4.5d \
    libopencv-imgproc4.5d \
    && rm -rf /var/lib/apt/lists/*

COPY --from=0 /src/build/dftfringe-cli /usr/local/bin/

ENTRYPOINT ["dftfringe-cli"]
