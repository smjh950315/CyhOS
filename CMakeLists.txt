cmake_minimum_required(VERSION 3.22)
project(cyhos)

set(GCC_COVERAGE_COMPILE_FLAGS "-fpermissive")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

# add include dir
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath -Wl,$ORIGIN -Wl,-fPIC")

# add cpp files
list(APPEND CYHCORE_SRCS
    "cyh/os/common_internal.cpp"
    "cyh/os/proc_mon.cpp"
    "cyh/os/res_mon.cpp"
    "cyh/os/shmem_mgr.cpp"
)

add_library(cyhos SHARED ${CYHCORE_SRCS})
set_target_properties(cyhos PROPERTIES OUTPUT_NAME cyhos)
target_compile_features(
    cyhos
    PUBLIC
    cxx_std_20
)
