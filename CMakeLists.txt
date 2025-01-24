cmake_minimum_required(VERSION 3.16)
project(distributed_system VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)

# Option to build specific components
option(BUILD_FRONTEND "Build frontend component" ON)
option(BUILD_MASTER "Build master component" ON)
option(BUILD_WORKER "Build worker component" ON)
option(BUILD_CLIENT "Build client component" ON)
OPTION(ENABLE_TESTS "Builds tests" ON)

# Add proto files first as they generate headers needed by other components
add_subdirectory(protos)

# Add components based on build options
if(BUILD_FRONTEND)
    add_subdirectory(src/frontend)
endif()

if(BUILD_MASTER)
    add_subdirectory(src/master)
endif()

if(BUILD_WORKER)
    add_subdirectory(src/worker)
endif()

if(BUILD_CLIENT)
    add_subdirectory(src/client)
endif()

if(ENABLE_TESTS)
    enable_testing()
    # Include GTest
    find_package(GTest CONFIG REQUIRED)

    add_subdirectory(tests)
endif()

