# Add test executables
find_package(GTest CONFIG REQUIRED)

add_executable(worker_tests worker/worker_tests.cpp)

target_link_libraries(worker_tests PRIVATE proto_lib worker GTest::gtest_main
        gRPC::grpc++_reflection gRPC::grpc++ protobuf::libprotobuf xxHash::xxhash)

include(GoogleTest)
