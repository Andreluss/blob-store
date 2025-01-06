# Add test executables
find_package(GTest CONFIG REQUIRED)
add_executable(echo_tests master/echo_tests.cpp)
target_link_libraries(echo_tests PRIVATE echo_service_proto master GTest::gtest_main gRPC::grpc++_reflection gRPC::grpc++ protobuf::libprotobuf)



#add_test(NAME EchoTests COMMAND echo_tests)
include(GoogleTest)
gtest_discover_tests(echo_tests)
