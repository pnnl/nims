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

# Find external packages in our vendorsrc
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

# link statically
set(Boost_USE_STATIC_LIBS   ON)
set(OpenCV_STATIC ON)
#set(YAMLCPP_STATIC_LIBRARY ON)

# find yaml
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries
#set(YAMLCPP_DIR ${CMAKE_SOURCE_DIR}/../vendorsrc/yaml-cpp-release-0.5.2)
set(Yaml-cpp_DIR ${CMAKE_SOURCE_DIR}/cmake/yaml)

# BlueView SDK
set(Bvtsdk_DIR "${CMAKE_SOURCE_DIR}/../vendorsrc/bvt_sdk_4.2.0.9446")
find_library(Bvtsdk_LIB bvtsdk ${Bvtsdk_DIR}/lib NO_DEFAULT_PATH)

# note, video component is needed for Kalman filter
find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc video)
# order of these matters for log_setup/log/thread
find_package(Boost REQUIRED COMPONENTS filesystem system program_options log_setup log thread filesystem)
find_package(Yaml-cpp REQUIRED)
MESSAGE(STATUS "YAML_CPP_LIBRARIES: ${YAML_CPP_LIBRARIES}")
find_package(Threads REQUIRED)


include_directories(${YAML_CPP_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Bvtsdk_DIR}/include)

set(Common_SOURCES log.cpp queues.cpp nims_ipc.cpp)

add_executable(ingester ingester.cpp data_source_m3.cpp data_source_ek60.cpp data_source_blueview.cpp frame_buffer.cpp ${Common_SOURCES})
add_executable(detector detector.cpp frame_buffer.cpp ${Common_SOURCES})
add_executable(tracker tracker.cpp frame_buffer.cpp tracked_object.cpp pixelgroup.cpp log.cpp nims_ipc.cpp)
add_executable(nims nims.cpp task.cpp ${Common_SOURCES})

target_link_libraries(ingester ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${YAML_CPP_LIBRARIES} ${Bvtsdk_LIB} rt)
target_link_libraries(detector ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} rt ${YAML_CPP_LIBRARIES})
target_link_libraries(tracker ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${YAML_CPP_LIBRARIES} rt)
target_link_libraries(nims ${Boost_LIBRARIES} ${YAML_CPP_LIBRARIES} rt)


ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:nims>)

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/../scripts/nims-init $<TARGET_FILE_DIR:nims>)
