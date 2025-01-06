# Generate gRPC and Protobuf files
set(PROTO_FILES
        services/echo_service.proto
        services/frontend_service.proto
)

# Generate protobuf and gRPC code
add_library(echo_service_proto
        ${PROTO_FILES}
)
target_link_libraries(echo_service_proto
        PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

protobuf_generate(TARGET echo_service_proto LANGUAGE cpp)
protobuf_generate(TARGET echo_service_proto LANGUAGE grpc
        GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
        PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)

# Make the generated files available to dependents
target_include_directories(echo_service_proto PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)