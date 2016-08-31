cmake_minimum_required(VERSION 3.3)

## ------- Configure your module here
project(offerfilterallocator)
set(MODULE_NAME im_getty_mesos_OfferFilteringAllocator)
# Match mesos version against which this build is targeted
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 1)
## ------- END: customization

execute_process(
    COMMAND git describe --tags --always
    OUTPUT_VARIABLE BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}-${BUILD_ID}")
message(STATUS "Building Version: ${VERSION_STRING}")

set(LIB_NAME lib${PROJECT_NAME}-${VERSION_STRING})
set(LIB_EXT ${CMAKE_SHARED_LIBRARY_SUFFIX})
set(MESOS_SRCS /mesos/src)

## ------- directories
set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/test)
set(TEMPLATE_DIR ${PROJECT_SOURCE_DIR}/template)
set(GEN_DIR ${PROJECT_SOURCE_DIR}/gen)

set(PROJECT_BINARY_DIR ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_PREFIX_PATH /usr/local)

## ------- shared configs
include_directories(
    ${GEN_DIR}
    ${PROJECT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
    ${MESOS_SRCS}
)
link_directories(/usr/local/lib)

set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -DPICOJSON_USE_INT64 -D__STDC_FORMAT_MACROS -std=c++11")

## ------- build module and tests
configure_file(${TEMPLATE_DIR}/README.md.in ${PROJECT_SOURCE_DIR}/README.md)
configure_file(${TEMPLATE_DIR}/config.h.in ${GEN_DIR}/config.h)
configure_file(${TEMPLATE_DIR}/modules.json.in ${GEN_DIR}/modules.json)

file(GLOB_RECURSE SRCS ${SOURCE_DIR}/*.cpp)
add_library(${PROJECT_NAME}-${VERSION_STRING} MODULE ${SRCS})

## ------- include tests
add_subdirectory(${TESTS_DIR})

message(STATUS "Building '${PROJECT_NAME}': ${PROJECT_SOURCE_DIR}")
