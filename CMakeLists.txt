cmake_minimum_required(VERSION 3.14)
project(mls VERSION 0.2.0 LANGUAGES C)

# ==== Project configuration ====

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug postfix: "mlsd.a" in debug, "mls.a" in release
set(CMAKE_DEBUG_POSTFIX "d")

option(MLS_THREAD_SAFE "Build with thread-safe locking" ON)
option(MLS_WERROR "Treat compiler warnings as errors" OFF)

# ==== Source files ====

set(MLS_SOURCES
    lib/mls.c
    lib/m_tool.c
    lib/m_table.c
    lib/m_extra.c
    lib/m_http.c
    lib/m_hdf.c
    lib/m_http_server.c
    lib/m_flask.c
)

set(MLS_PUBLIC_HEADERS
    lib/mls.h
    lib/m_tool.h
    lib/m_table.h
    lib/m_extra.h
    lib/m_http.h
    lib/m_hdf.h
    lib/m_http_server.h
    lib/m_flask.h
)

# ==== Library target ====

add_library(mls STATIC ${MLS_SOURCES})

target_include_directories(mls
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
            $<INSTALL_INTERFACE:include/mls>
    PRIVATE lib
)

target_compile_definitions(mls PRIVATE _GNU_SOURCE)

# Debug mode: sanitizers + MLS_DEBUG
set(MLS_DEBUG_FLAGS -g -DMLS_DEBUG -fstack-protector-all -fsanitize=address)
target_compile_options(mls PRIVATE
    $<$<CONFIG:Debug>:${MLS_DEBUG_FLAGS}>
)
target_link_options(mls PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# Release mode: optimisation
target_compile_options(mls PRIVATE
    $<$<CONFIG:Release>:-O3 -fdata-sections -ffunction-sections>
)
target_link_options(mls PRIVATE
    $<$<CONFIG:Release>:-Wl,--gc-sections>
)

if(MLS_WERROR)
    target_compile_options(mls PRIVATE -Werror)
endif()

# Thread safety
if(MLS_THREAD_SAFE)
    target_compile_definitions(mls PRIVATE MLS_THREAD_SAFE)
    find_package(Threads REQUIRED)
    target_link_libraries(mls PUBLIC Threads::Threads)
endif()

# Link maths and dl (used by some modules)
target_link_libraries(mls PUBLIC m dl)

# ==== Install rules ====

include(GNUInstallDirs)

install(TARGETS mls
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${MLS_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mls
)

# ==== pkg-config support ====

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/mls.pc.in"
    "${CMAKE_BINARY_DIR}/mls.pc"
    @ONLY
)

install(FILES "${CMAKE_BINARY_DIR}/mls.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
