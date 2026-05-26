cmake_minimum_required(VERSION 3.16)
project(radarBench C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

set(ARMPL_DIR "/opt/arm/arm-performance-libraries" CACHE PATH "ARM Performance Libraries root")

add_compile_options(
    -O3
    -ffast-math
    -fcx-limited-range
    -Wall
    -Wdouble-promotion
    -D_GNU_SOURCE
)

set(SOURCES
    Src/radarBench.c
    Src/loader.c
    Src/pulse.c
    Src/doppler_fft.c
    Src/cfar.c
    Src/print.c
    Src/cluster.c
    Src/udp.c
    Src/send_graph_data.c

    Src/settings/core_set.c
    Src/settings/pipeline_set.c

    Src/thread/loader_thread.c
    Src/thread/pulse_compress_thread.c
    Src/thread/doppler_cfar_thread.c

    Src/queue/queue_interface.c
    Src/queue/futex/pulse_queue_fu.c
    Src/queue/futex/post_queue_fu.c
    Src/queue/lock_free/pulse_queue_lf.c
    Src/queue/lock_free/post_queue_lf.c
    Src/queue/mutex/pulse_queue_mu.c
    Src/queue/mutex/post_queue_mu.c
    Src/queue/usleep/pulse_queue_us.c
    Src/queue/usleep/post_queue_us.c

    Src/transpose/transpose_interface.c
    Src/transpose/transpose_tiling.c
    Src/transpose/transpose_comatcopy.c

    Src/dispatch/dispatch_interface.c
    Src/dispatch/dispatch_half.c
    Src/dispatch/dispatch_evenodd.c

    Src/layout/layout_interface.c
    Src/layout/layout_default.c
    Src/layout/layout_legacy.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    Inc
    Inc/thread
    Inc/settings
    Inc/queue
    Inc/queue/futex
    Inc/queue/lock_free
    Inc/queue/mutex
    Inc/queue/usleep
    Inc/transpose
    Inc/dispatch
    Inc/layout

    ${ARMPL_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

target_link_directories(${PROJECT_NAME} PRIVATE ${ARMPL_DIR}/lib)
target_link_libraries(${PROJECT_NAME} PRIVATE
    armpl_lp64
    fftw3f
    m
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH   "${ARMPL_DIR}/lib"
    INSTALL_RPATH "${ARMPL_DIR}/lib"
)