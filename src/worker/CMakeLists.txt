set(COMPONENT_NAME worker)

add_executable(${COMPONENT_NAME}
        main.cpp
)

target_include_directories(${COMPONENT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(${COMPONENT_NAME}
        PRIVATE
        echo_service_proto
        gRPC::grpc++
        gRPC::grpc++_reflection
)

# Set output directory
set_target_properties(${COMPONENT_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
