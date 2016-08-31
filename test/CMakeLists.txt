project(${PROJECT_NAME}_test)

enable_testing()

file(GLOB_RECURSE TEST_SRCS ${TESTS_DIR}/*.cpp)

set(TEST_TARGET ${PROJECT_NAME})

add_executable(${TEST_TARGET} ${SRCS} ${TEST_SRCS})

target_link_libraries(${TEST_TARGET}
    gtest gtest_main
    protobuf
    mesos
    pthread
    glog)

add_test(${TEST_TARGET} ${CMAKE_CURRENT_BINARY_DIR}/${TEST_TARGET})
