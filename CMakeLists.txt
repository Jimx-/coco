cmake_minimum_required(VERSION 3.5)
project(coco)

set(CMAKE_CXX_STANDARD 17)

option(COCO_BUILD_TESTS "set ON to build library tests" OFF)

set(TOPDIR ${PROJECT_SOURCE_DIR})

set(INCLUDE_DIRS
    ${TOPDIR}/include
)

include_directories(
    ${INCLUDE_DIRS}
)

set(SOURCE_FILES
    ${TOPDIR}/src/coco.cpp
    ${TOPDIR}/src/io_context.cpp
    ${TOPDIR}/src/io_poller.cpp
    ${TOPDIR}/src/scheduler.cpp
    ${TOPDIR}/src/sync/condition_variable.cpp
    ${TOPDIR}/src/sync/mutex.cpp
    ${TOPDIR}/src/sync/shared_mutex.cpp        
    ${TOPDIR}/src/syscalls.cpp
    ${TOPDIR}/src/task.cpp
    ${TOPDIR}/src/thread_context.cpp
)
            
set(HEADER_FILES
    ${TOPDIR}/include/coco/coco.h
    ${TOPDIR}/include/coco/io_context.h
    ${TOPDIR}/include/coco/io_poller.h        
    ${TOPDIR}/include/coco/scheduler.h
    ${TOPDIR}/include/coco/stackframe.h
    ${TOPDIR}/include/coco/sync/condition_variable.h
    ${TOPDIR}/include/coco/sync/mutex.h
    ${TOPDIR}/include/coco/sync/shared_mutex.h                
    ${TOPDIR}/include/coco/sync/spinlock.h
    ${TOPDIR}/include/coco/sync.h    
    ${TOPDIR}/include/coco/syscalls.h
    ${TOPDIR}/include/coco/task.h
    ${TOPDIR}/include/coco/thread_context.h
)

set(EXT_SOURCE_FILES )

set(LIBRARIES
    pthread
    dl
)
 
add_library(coco STATIC ${SOURCE_FILES} ${HEADER_FILES} ${EXT_SOURCE_FILES})
target_link_libraries(coco ${LIBRARIES})
target_include_directories(coco PUBLIC ${INCLUDE_DIRS})
install(TARGETS coco DESTINATION lib)

if (COCO_BUILD_TESTS)
add_subdirectory(${TOPDIR}/3rdparty/googletest/googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(TEST_SOURCE_FILES
    ${TOPDIR}/tests/coco_test.cpp)
    
add_executable(coco_unit_tests ${EXT_SOURCE_FILES} ${TEST_SOURCE_FILES})
target_link_libraries(coco_unit_tests coco gtest gtest_main ${LIBRARIES})
add_test(coco_tests coco_unit_tests)
endif()
