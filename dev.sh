#!/bin/bash

# Save as dev.sh and make executable with: chmod +x dev.sh

# Default values
PROJECT_NAME="blob-store"
WORKDIR_PATH=/usr/src/app
COMMAND=""

# Help message
show_help() {
    echo "Usage: ./dev.sh [command]"
    echo ""
    echo "Commands:"
    echo "  build-image - Build development container"
    echo "  shell       - Start a development shell in Docker"
    echo "  build       - Build the project"
    echo "  rebuild     - Clean and rebuild the project"
    echo "  run-fe      - Run the frontend service"
    echo "  run-master  - Run the master service"
    echo "  run-worker  - Run the worker service"
    echo "  test        - Run tests"
    echo "  clean       - Clean build directory"
    echo "  help        - Show this help message"
}

build_image() {
    docker build -t ${PROJECT_NAME} --build-arg workdir=${WORKDIR_PATH} . -f build.Dockerfile
}

# Ensure build directory exists
ensure_build_dir() {
    mkdir -p build
}

# Start development shell
dev_shell() {
    ensure_build_dir
    docker run -it --rm \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} bash
}

# Build project
build() {
    ensure_build_dir
    docker run -it --rm \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} \
        bash -c "cmake -B build -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build"
}

# Clean build
clean() {
    rm -rf build/*
}

# Rebuild project
rebuild() {
    clean
    build
}

# Run services
run_frontend() {
    docker run -it --rm \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} &
    ./build/bin/frontend
}

run_master() {
    docker run -it --rm \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} &
    ./build/bin/master
}

run_worker() {
    docker run -it --rm \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} &
    ./build/bin/worker
}

run_tests() {
    docker run --rm -it \
        -v $(pwd):${WORKDIR_PATH} \
        -v $(pwd)/build:${WORKDIR_PATH}/build \
        ${PROJECT_NAME} sh -c "cd build && ctest"
}

# Parse command
case "$1" in
    "shell")
        dev_shell
        ;;
    "build-image")
        build_image
        ;;
    "build")
        build
        ;;
    "rebuild")
        rebuild
        ;;
    "clean")
        clean
        ;;
    "run-fe")
        run_frontend
        ;;
    "run-master")
        run_master
        ;;
    "run-worker")
        run_worker
        ;;
    "test")
        run_tests
        ;;
    "help"|"")
        show_help
        ;;
    *)
        echo "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
