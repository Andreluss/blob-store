# Set the library name
set(COMPONENT_NAME master)

# Add the source files for the frontend component
add_library(${COMPONENT_NAME} STATIC
        master_service.hpp
        master_service.cpp
)


# Master db repository lib
add_library(master_db_repo STATIC master_db_repository.hpp master_db_repository.cpp)
target_include_directories(master_db_repo PUBLIC
        ${CMAKE_SOURCE_DIR}/src/master
        ${CMAKE_SOURCE_DIR}/src/common
)
target_link_libraries(master_db_repo PUBLIC Boost::uuid google-cloud-cpp::spanner)

# Master db example
add_executable(master_db_example master_db_example.cpp)
target_link_libraries(master_db_example PUBLIC
        master_db_repo
        google-cloud-cpp::spanner
)


# Specify include directories
target_include_directories(${COMPONENT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/protos
        ${CMAKE_SOURCE_DIR}/src/master
        ${CMAKE_SOURCE_DIR}/src/common
)

# Link dependencies
target_link_libraries(${COMPONENT_NAME} PUBLIC
        proto_lib
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
        google-cloud-cpp::spanner
        Boost::uuid
        master_db_repo
)

add_executable(master_server main.cpp
        master_service_mock.hpp)
target_link_libraries(master_server PRIVATE
        ${COMPONENT_NAME}
        google-cloud-cpp::spanner
)

set_target_properties(master_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)