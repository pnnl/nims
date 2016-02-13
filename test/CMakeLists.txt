cmake_minimum_required (VERSION 2.6)
project (NIMS_TEST) 

# TODO:  Eliminate unnecessary stuff.

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

# set path to main source dir
set(NIMS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../nims-source)

# Find external packages in our vendorsrc
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${NIMS_SOURCE_DIR}/cmake/Modules")

# link statically
set(Boost_USE_STATIC_LIBS   ON)
set(OpenCV_STATIC ON)
set(YAMLCPP_STATIC_LIBRARY ON)

#set (YAMLCPP_DIR ${CMAKE_SOURCE_DIR}/../vendorsrc/yaml-cpp-release-0.5.2)

set(Bvtsdk_DIR "${CMAKE_SOURCE_DIR}/../vendorsrc/bvt_sdk_4.2.0.9446")
find_library(Bvtsdk_LIB bvtsdk ${Bvtsdk_DIR}/lib NO_DEFAULT_PATH)

find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc video)
# order of these matters for log_setup/log/thread
find_package(Boost REQUIRED COMPONENTS filesystem system program_options log_setup log thread filesystem)
find_package(Yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

include_directories(${YAMLCPP_INCLUDE_DIR})
include_directories(${Bvtsdk_DIR}/include)
include_directories(${NIMS_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR})

set(COMMON_SOURCES ${NIMS_SOURCE_DIR}/frame_buffer.cpp ${NIMS_SOURCE_DIR}/nims_ipc.cpp ${NIMS_SOURCE_DIR}/log.cpp)

add_executable(test_frame_buffer_put test_frame_buffer_put.cpp ${COMMON_SOURCES})
add_executable(test_frame_buffer_get test_frame_buffer_get.cpp ${COMMON_SOURCES})
add_executable(test_frame_buffer test_frame_buffer.cpp ${NIMS_SOURCE_DIR}/log.cpp)
add_executable(test_blueview test_blueview.cpp ${NIMS_SOURCE_DIR}/data_source_blueview.cpp ${COMMON_SOURCES})
add_executable(test_ek60 test_ek60.cpp ${NIMS_SOURCE_DIR}/data_source_ek60.cpp ${COMMON_SOURCES})


target_link_libraries(test_frame_buffer_put ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} rt)
target_link_libraries(test_frame_buffer_get ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} rt)
target_link_libraries(test_frame_buffer ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} rt)
target_link_libraries(test_blueview ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} ${OpenCV_LIBRARIES} ${Bvtsdk_LIB} rt)
target_link_libraries(test_ek60 ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} ${OpenCV_LIBRARIES} ${Bvtsdk_LIB} rt)


ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NIMS_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:nims>)

