cmake_minimum_required(VERSION 3.14)
project(mls VERSION 0.2.0 LANGUAGES C)

# ==== Project configuration ====

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Debug postfix: "mls_based.a" / "mls_extd.a" in debug, "mls_base.a" / "mls_ext.a" in release
set(CMAKE_DEBUG_POSTFIX "d")

option(MLS_THREAD_SAFE "Build with thread-safe locking" ON)
option(MLS_WERROR "Treat compiler warnings as errors" OFF)

# ==== Source files ====

set(MLS_BASE_SOURCES
    lib/mls_base.c
)

# Everything that can be omitted by a program that only wants dynamic arrays.
set(MLS_EXT_SOURCES
    lib/mls_ext.c
    lib/m_tool.c
    lib/list.c
    lib/table.c
    lib/m_extra.c
    lib/m_http.c
    lib/m_hdf.c
    lib/m_http_server.c
    lib/m_flask.c
)

set(MLS_PUBLIC_HEADERS
    lib/mls.h
    lib/mls_base.h
    lib/mls_ext.h
    lib/m_tool.h
    lib/list.h
    lib/table.h
    lib/m_extra.h
    lib/m_http.h
    lib/m_hdf.h
    lib/m_http_server.h
    lib/m_flask.h
)

# ==== Library targets ====

add_library(mls_base STATIC ${MLS_BASE_SOURCES})
add_library(mls_ext STATIC ${MLS_EXT_SOURCES})
target_link_libraries(mls_ext PUBLIC mls_base)

foreach(tgt mls_base mls_ext)
    target_include_directories(${tgt}
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
                $<INSTALL_INTERFACE:include/mls>
        PRIVATE lib
    )

    target_compile_definitions(${tgt} PRIVATE _GNU_SOURCE)

    # Debug mode: sanitizers + MLS_DEBUG
    target_compile_options(${tgt} PRIVATE
        $<$<CONFIG:Debug>:-g -DMLS_DEBUG -fstack-protector-all -fsanitize=address>
    )
    target_link_options(${tgt} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
    )

    # Release mode: optimisation
    target_compile_options(${tgt} PRIVATE
        $<$<CONFIG:Release>:-O3 -fdata-sections -ffunction-sections>
    )
    target_link_options(${tgt} PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
    )

    if(MLS_WERROR)
        target_compile_options(${tgt} PRIVATE -Werror)
    endif()

    if(MLS_THREAD_SAFE)
        target_compile_definitions(${tgt} PUBLIC MLS_THREAD_SAFE=1)
        find_package(Threads REQUIRED)
        target_link_libraries(${tgt} PUBLIC Threads::Threads)
    else()
        target_compile_definitions(${tgt} PUBLIC MLS_THREAD_SAFE=0)
    endif()

    # Link maths and dl (used by some modules)
    target_link_libraries(${tgt} PUBLIC m dl)
endforeach()

# ==== Install rules ====

include(GNUInstallDirs)

install(TARGETS mls_base mls_ext
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${MLS_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mls
)

# ==== pkg-config support ====

set(MLS_PC_PRIVATE_LIBS "")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/mls.pc.in"
    "${CMAKE_BINARY_DIR}/mls.pc"
    @ONLY
)

install(FILES "${CMAKE_BINARY_DIR}/mls.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
