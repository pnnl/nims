cmake_minimum_required (VERSION 2.6) 
project (NIMS) 

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# link Boost statically
set(Boost_USE_STATIC_LIBS   ON)

# enable extended initializer lists
#add_definitions("-std=c++0x")

# find yaml cmake config
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(Yaml-cpp_DIR ${CMAKE_SOURCE_DIR}/cmake/Modules)

#find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)
find_package(Boost REQUIRED COMPONENTS filesystem system program_options)
find_package(Yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

#include_directories(${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${YAML_CPP_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})

add_executable(ingester ingester.cpp data_source_m3.cpp data_source.cpp frame_buffer.cpp)
add_executable(detector detector.cpp queues.cpp frame_buffer.cpp)
add_executable(tracker tracker.cpp queues.cpp frame_buffer.cpp)
add_executable(nims nims.cpp queues.cpp task.cpp)
add_executable(test_frame_buffer_put test_frame_buffer_put.cpp frame_buffer.cpp)
add_executable(test_frame_buffer_get test_frame_buffer_get.cpp frame_buffer.cpp)
add_executable(test_frame_buffer test_frame_buffer.cpp)

#target_link_libraries(ingester ${OpenCV_LIBS} ${Boost_LIBRARIES})
# Can't figure out which finder has Linux realtime, which
# we need for POSIX shared memory. Adding rt breaks cross-platform.
target_link_libraries(ingester ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt)

#target_link_libraries(detector ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(detector ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt)

#target_link_libraries(tracker ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(tracker ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt)

target_link_libraries(nims ${Boost_LIBRARIES} ${YAML_CPP_LIBRARIES} rt)
target_link_libraries(test_frame_buffer_put ${Boost_LIBRARIES} rt)
target_link_libraries(test_frame_buffer_get ${Boost_LIBRARIES} rt)

# only copies once
# file(COPY ${PROJECT_SOURCE_DIR}/config.yaml
#     DESTINATION ${CMAKE_BINARY_DIR}
# )

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:nims>)

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/../scripts/nims.sh $<TARGET_FILE_DIR:nims>)
