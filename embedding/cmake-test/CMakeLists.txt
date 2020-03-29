cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0091 NEW)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(CMAKE_CXX_STANDARD 11)
project(node_embedding_test)

include(FetchContent)

add_subdirectory(${NODE_CMAKE_DIR} node)

enable_testing()

foreach(TEST_NAME IN ITEMS test_blocking test_embedded_binding test_loop_timer test_loop_io test_loop_worker)
    message(${TEST_NAME})
	add_executable(${TEST_NAME} ${TEST_NAME}.cc)
	target_link_libraries(${TEST_NAME} node)
	add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
