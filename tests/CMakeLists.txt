# Add test executables
find_package(GTest CONFIG REQUIRED)

add_executable(echo_tests master/echo_tests.cpp)
add_executable(worker_tests worker/worker_tests.cpp)

target_link_libraries(echo_tests PRIVATE proto_lib master GTest::gtest_main gRPC::grpc++_reflection gRPC::grpc++ protobuf::libprotobuf)
target_link_libraries(worker_tests PRIVATE proto_lib worker GTest::gtest_main
        gRPC::grpc++_reflection gRPC::grpc++ protobuf::libprotobuf xxHash::xxhash)



#add_test(NAME EchoTests COMMAND echo_tests)
include(GoogleTest)
gtest_discover_tests(echo_tests)
