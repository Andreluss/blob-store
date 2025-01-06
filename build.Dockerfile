FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install basic build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    ninja-build \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg for managing dependencies
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh && \
    ln -s /opt/vcpkg/vcpkg /usr/local/bin/vcpkg

# Install gRPC and Protobuf using vcpkg
RUN vcpkg install grpc:x64-linux protobuf:x64-linux

# Set up working directory
ARG workdir
WORKDIR $workdir

# Copy project files
COPY . .

# Configure and build
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake && \
    cmake --build build --parallel

# Set the default command
CMD ["./build/bin/frontend"]