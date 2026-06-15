cmake_minimum_required(VERSION 3.21)

project(siecs_consumer_c LANGUAGES C)

enable_testing()

add_subdirectory(../../.. siecs-build)

add_executable(siecs_consumer_c main.c)
target_compile_features(siecs_consumer_c PRIVATE c_std_23)
target_link_libraries(siecs_consumer_c PRIVATE siecs::siecs)

add_test(NAME siecs_consumer_c COMMAND siecs_consumer_c)
