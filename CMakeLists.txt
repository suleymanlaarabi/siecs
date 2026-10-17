cmake_minimum_required(VERSION 3.25)
project(siecs LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

find_package(sicore CONFIG REQUIRED)
find_package(sireflect CONFIG REQUIRED)
find_package(sijson CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(siecs
    src/bootstrap.c
    src/command_buffer.c
    src/component.c
    src/component_ops.c
    src/entity.c
    src/inheritance.c
    src/module.c
    src/observer.c
    src/platform.c
    src/query.c
    src/reflect.c
    src/relation.c
    src/resource.c
    src/save.c
    src/system.c
    src/table.c
    src/table_migration.c
    src/type.c
    src/worker_pool.c
    src/world.c
    src/datastructure/arena.c
    src/datastructure/idmap.c
    src/storage/component_index.c
    src/storage/query_index.c
    src/storage/system_index.c
    src/storage/table_index.c)
add_library(siecs::siecs ALIAS siecs)
target_compile_features(siecs PUBLIC c_std_17)
set_target_properties(siecs PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(siecs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(siecs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(siecs PRIVATE siecs_EXPORTS)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(siecs PUBLIC siecs_STATIC)
endif()
target_link_libraries(siecs PUBLIC sicore::sicore sireflect::sireflect sijson::sijson Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_compile_definitions(siecs PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(siecs PRIVATE m)
    # ECS declarations in applications must remain visible to native symbol lookup.
    target_link_options(siecs INTERFACE "$<$<PLATFORM_ID:Linux>:-Wl,--export-dynamic>")
endif()
option(SIECS_BUILD_SPATIAL "Build the spatial addon" OFF)
if(SIECS_BUILD_SPATIAL)
    add_library(siecs_spatial addons/spatial/src/spatial.c)
    add_library(siecs::spatial ALIAS siecs_spatial)
    set_target_properties(siecs_spatial PROPERTIES EXPORT_NAME spatial)
    target_compile_features(siecs_spatial PUBLIC c_std_17)
    target_include_directories(siecs_spatial PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/addons/spatial/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(siecs_spatial PUBLIC siecs::siecs)
    if(UNIX)
        target_link_libraries(siecs_spatial PRIVATE m)
    endif()
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(siecs_spatial PUBLIC siecs_spatial_STATIC)
    endif()
    install(TARGETS siecs_spatial EXPORT siecsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES addons/spatial/include/siecs_spatial.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install(TARGETS siecs EXPORT siecsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/siecs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/siecs/config.h include/siecs/reflect_internal.h include/siecs/cpp.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/siecs)
install(DIRECTORY include/siecs/cpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/siecs)
install(EXPORT siecsTargets NAMESPACE siecs:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/siecs)
configure_package_config_file(cmake/siecsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/siecsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/siecs)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/siecsConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/siecs)

