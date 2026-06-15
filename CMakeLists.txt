cmake_minimum_required(VERSION 3.21)

project(siecs VERSION 0.1.0 LANGUAGES C)

include(FetchContent)

option(SIECS_REST "Build SIECS with the REST addon enabled" ON)
option(SIECS_BUILD_CPP "Expose the header-only C++ wrapper target" ${CMAKE_CXX_COMPILER_LOADED})
option(SIECS_BUILD_TESTS "Build SIECS CMake tests" OFF)
option(SIECS_BUILD_EXAMPLES "Build SIECS CMake examples" OFF)

function(siecs_fetch_c_dependency dep_name repository)
    if(TARGET ${dep_name})
        return()
    endif()

    FetchContent_Declare(
        ${dep_name}
        GIT_REPOSITORY ${repository}
        GIT_TAG main
    )

    FetchContent_GetProperties(${dep_name})
    if(NOT ${dep_name}_POPULATED)
        FetchContent_Populate(${dep_name})
    endif()

    file(GLOB dep_sources CONFIGURE_DEPENDS
        "${${dep_name}_SOURCE_DIR}/src/*.c"
    )

    add_library(${dep_name} STATIC ${dep_sources})
    add_library(${dep_name}::${dep_name} ALIAS ${dep_name})

    target_compile_features(${dep_name} PUBLIC c_std_23)
    target_compile_definitions(${dep_name}
        PUBLIC
            ${dep_name}_STATIC
        PRIVATE
            _POSIX_C_SOURCE=200809L
    )
    target_include_directories(${dep_name}
        PUBLIC
            "$<BUILD_INTERFACE:${${dep_name}_SOURCE_DIR}/include>"
        PRIVATE
            "${${dep_name}_SOURCE_DIR}/src"
    )
endfunction()

siecs_fetch_c_dependency(sireflect https://github.com/suleymanlaarabi/sireflect.git)
siecs_fetch_c_dependency(sijson https://github.com/suleymanlaarabi/sijson.git)
siecs_fetch_c_dependency(sihttp https://github.com/suleymanlaarabi/sihttp.git)

target_link_libraries(sijson PUBLIC sireflect)
target_link_libraries(sihttp PUBLIC sijson)

file(GLOB_RECURSE siecs_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

if(NOT SIECS_REST)
    list(FILTER siecs_sources EXCLUDE REGEX "/src/addons/rest\\.c$")
endif()

add_library(siecs STATIC ${siecs_sources})
add_library(siecs::siecs ALIAS siecs)

target_compile_features(siecs PUBLIC c_std_23)
target_compile_definitions(siecs
    PUBLIC
        siecs_STATIC
    PRIVATE
        _POSIX_C_SOURCE=200809L
        $<$<BOOL:${SIECS_REST}>:SIECS_REST>
)
target_include_directories(siecs
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(siecs
    PUBLIC
        sireflect
        sijson
        sihttp
)

if(UNIX AND NOT APPLE)
    target_link_libraries(siecs PUBLIC pthread)
endif()

if(SIECS_BUILD_CPP)
    enable_language(CXX)

    add_library(siecs_cpp INTERFACE)
    add_library(siecs::siecs_cpp ALIAS siecs_cpp)

    target_compile_features(siecs_cpp INTERFACE cxx_std_23)
    target_include_directories(siecs_cpp
        INTERFACE
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/addons/siecs_cpp/include>"
    )
    target_link_libraries(siecs_cpp INTERFACE siecs)
endif()

if(SIECS_BUILD_TESTS)
    enable_testing()
endif()
