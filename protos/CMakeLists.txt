# Generate gRPC and Protobuf files
set(PROTO_FILES
        services/echo_service.proto
        services/frontend_service.proto
        services/master_service.proto
        services/worker_service.proto
        common.proto
)

# Generate protobuf and gRPC code
add_library(proto_lib
        ${PROTO_FILES}
)
target_link_libraries(proto_lib
        PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

protobuf_generate(TARGET proto_lib LANGUAGE cpp
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
protobuf_generate(TARGET proto_lib LANGUAGE grpc
        GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
        PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
        PLUGIN_OPTIONS generate_mock_code=true
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# Make the generated files available to dependents
target_include_directories(proto_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)