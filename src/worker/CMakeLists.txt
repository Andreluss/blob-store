set(COMPONENT_NAME worker)


add_library(${COMPONENT_NAME} STATIC
        worker_service.cpp
)

target_include_directories(${COMPONENT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/worker
)

target_link_libraries(${COMPONENT_NAME} PUBLIC
        proto_lib
        gRPC::grpc++
        gRPC::grpc++_reflection
        xxHash::xxhash
)

add_executable(worker_server main.cpp)
target_link_libraries(worker_server PRIVATE ${COMPONENT_NAME})

# Set output directory
set_target_properties(worker_server
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
