FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    git \
    libpng-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Install official DragonMinded prebuilt MIPS64 GCC toolchain (native ARM64!)
RUN curl -fsSL -o /tmp/toolchain.deb \
    https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-arm64.deb \
    && apt-get update \
    && apt-get install -y /tmp/toolchain.deb \
    && rm -f /tmp/toolchain.deb \
    && rm -rf /var/lib/apt/lists/*

ENV N64_INST=/opt/libdragon
ENV PATH="${N64_INST}/bin:${PATH}"

# Clone and install libdragon preview branch (OpenGL, RDPQ, mixer, wav64, mksprite, audioconv64)
RUN git clone --depth 1 -b preview https://github.com/DragonMinded/libdragon.git /opt/libdragon-src \
    && cd /opt/libdragon-src \
    && make -j$(nproc) \
    && make install \
    && make tools -j$(nproc) \
    && make tools-install \
    && rm -rf /opt/libdragon-src

# Clone and install Tiny3D (native fixed-point RSP 3D microcode + TPX particles)
RUN git clone --depth 1 https://github.com/HailToDodongo/tiny3d.git /opt/tiny3d-src \
    && cd /opt/tiny3d-src \
    && make -j$(nproc) \
    && make install \
    && rm -rf /opt/tiny3d-src

WORKDIR /work
