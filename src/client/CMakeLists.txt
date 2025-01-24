set(COMPONENT_NAME client)

# Add the source files for the frontend component
add_executable(${COMPONENT_NAME}
        main.cpp
        client.cpp
)

# Specify include directories
target_include_directories(${COMPONENT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/src/common
)

# Link dependencies
target_link_libraries(${COMPONENT_NAME}
        PRIVATE
        proto_lib
        gRPC::grpc++
        gRPC::grpc++_reflection
        xxHash::xxhash
)

set_target_properties(${COMPONENT_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
