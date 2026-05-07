FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    g++ \
    libomp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . /workspace

# Linux build is expected to fail at compile stage because CAM-Expert is a Win32 app,
# but configure and toolchain resolution are reproducible in CI with this image.
RUN cmake -S . -B build -G Ninja

CMD ["cmake", "--build", "build", "-j2"]

