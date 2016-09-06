cmake_minimum_required(VERSION 3.3)

## ------- Configure your module here
project(offerfilterallocator)
set(MODULE_NAME im_getty_mesos_OfferFilteringAllocator)
# Match mesos version against which this build is targeted
set(MESOS_VERSION 1.0.1)
## ------- END: customization

execute_process(
    COMMAND git describe --tags --always
    OUTPUT_VARIABLE BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VERSION_STRING "${MESOS_VERSION}-${BUILD_ID}")
message(STATUS "Building Version: ${VERSION_STRING}")

set(LIB_NAME lib${PROJECT_NAME}-${VERSION_STRING})
set(LIB_EXT ${CMAKE_SHARED_LIBRARY_SUFFIX})

## ------- directories
set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/test)
set(TEMPLATE_DIR ${PROJECT_SOURCE_DIR}/template)
set(GEN_DIR ${PROJECT_SOURCE_DIR}/gen)

set(PROJECT_BINARY_DIR ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_PREFIX_PATH /usr/local)

# find_package(Protobuf REQUIRED)

## ------- shared configs
add_definitions(-DPROTOBUF_USE_DLLS)

include_directories(
    ${GEN_DIR}
    ${PROJECT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    /mesos/src
    /mesos/build/3rdparty/zookeeper-3.4.8/src/c/generated
    /mesos/build/3rdparty/zookeeper-3.4.8/src/c/include
    #/usr/include/zookeeper
)

# /mesos/build/3rdparty/zookeeper-3.4.8/src/c/generated
# /mesos/build/3rdparty/zookeeper-3.4.8/src/c/include
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
