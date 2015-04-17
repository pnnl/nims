cmake_minimum_required (VERSION 2.6) 
project (NIMS) 

# link Boost statically
set(Boost_USE_STATIC_LIBS   ON)

# enable extended initializer lists
add_definitions("-std=c++0x")

# find yaml cmake config
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(Yaml-cpp_DIR ${CMAKE_SOURCE_DIR}/cmake/Modules)

#find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc video)
find_package(Boost REQUIRED COMPONENTS filesystem system program_options)
find_package(Yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

#include_directories(${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${YAML_CPP_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})

add_executable(ingester ingester.cpp)
add_executable(detector detector.cpp)
add_executable(tracker tracker.cpp)
add_executable(nims nims.cpp)

#target_link_libraries(ingester ${OpenCV_LIBS} ${Boost_LIBRARIES})
# Can't figure out which finder has Linux realtime, which
# we need for POSIX shared memory. Adding rt breaks cross-platform.
target_link_libraries(ingester ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt)

#target_link_libraries(detector ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(detector ${Boost_LIBRARIES})

#target_link_libraries(tracker ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(tracker ${Boost_LIBRARIES})

target_link_libraries(nims ${Boost_LIBRARIES} ${YAML_CPP_LIBRARIES} rt)

# only copies once
# file(COPY ${PROJECT_SOURCE_DIR}/config.yaml
#     DESTINATION ${CMAKE_BINARY_DIR}
# )

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:nims>)
