cmake_minimum_required(VERSION 3.21)

project(siecs_consumer_cpp LANGUAGES C CXX)

enable_testing()

add_subdirectory(../../.. siecs-build)

add_executable(siecs_consumer_cpp main.cpp)
target_compile_features(siecs_consumer_cpp PRIVATE cxx_std_23)
target_link_libraries(siecs_consumer_cpp PRIVATE siecs::siecs_cpp)

add_test(NAME siecs_consumer_cpp COMMAND siecs_consumer_cpp)
