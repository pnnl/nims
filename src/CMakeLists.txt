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
# NOTE: the cmake module path is where Find<package>.cmake files are found.
#       cmake comes with many Find<package>.cmake files; others can sometimes
#       be found online.  They set the variables <package>_LIBRARIES and 
#       <package>_INCLUDE_DIRS.
#       We need to supply one for yaml because CMake does not come with one.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

# link statically
# These need to be set before calling find_package because the
# Find<package>.cmake files will check this var to set the right
# library path and confirm the library exists.
set(Boost_USE_STATIC_LIBS   ON)
set(OpenCV_STATIC ON)
set(YAMLCPP_STATIC_LIBRARY ON)

# BlueView SDK
set(Bvtsdk_DIR "${CMAKE_SOURCE_DIR}/../vendorsrc/bvt_sdk_4.2.0.9446")
find_library(Bvtsdk_LIB bvtsdk ${Bvtsdk_DIR}/lib NO_DEFAULT_PATH)

# NOTE: video component is needed for Kalman filter
find_package(OpenCV REQUIRED COMPONENTS core highgui imgcodecs imgproc video)

# NOTE: order of these matters for log_setup/log/thread, filesystem needs to be at the end
find_package(Boost REQUIRED COMPONENTS system program_options log_setup log thread filesystem date_time)

# NOTE: If your yaml is in a non-standard place (see nims-source/cmake/Modules/FindYaml-cpp.cmake 
#       for the standard places), then uncomment the following line and
#       edit accordingly.  The file libyaml-cpp.a must be in ${YAMLCPP_DIR}/lib and the 
#       include files must be in ${YAMLCPP_DIR}/include.
#set (YAMLCPP_DIR ${CMAKE_SOURCE_DIR}/../vendorsrc/yaml-cpp-release-0.5.2)
find_package(Yaml-cpp REQUIRED)

find_package(Threads REQUIRED)


include_directories(${YAMLCPP_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Bvtsdk_DIR}/include)

set(Common_SOURCES log.cpp nims_ipc.cpp)
set(data_SOURCES data_source_m3.cpp data_source_ek60.cpp data_source_blueview.cpp data_source_amp-m3.cpp)

add_executable(ingester ingester.cpp frame_buffer.cpp ${data_SOURCES} ${Common_SOURCES})
add_executable(detector detector.cpp pixelgroup.cpp frame_buffer.cpp ${Common_SOURCES})
add_executable(tracker tracker.cpp tracked_object.cpp ${Common_SOURCES})
add_executable(nims nims.cpp task.cpp ${Common_SOURCES})
add_executable(m3sim m3sim.cpp )
add_executable(viewer viewer.cpp frame_buffer.cpp ${Common_SOURCES})

target_link_libraries(ingester ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${YAMLCPP_LIBRARY} ${Bvtsdk_LIB} rt)
target_link_libraries(detector ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} rt ${YAMLCPP_LIBRARY})
target_link_libraries(tracker ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${YAMLCPP_LIBRARY} rt)
target_link_libraries(nims ${Boost_LIBRARIES} ${YAMLCPP_LIBRARY} rt)
target_link_libraries(viewer ${Boost_LIBRARIES} ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${YAMLCPP_LIBRARY} rt)

add_subdirectory(nims_py)

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config.yaml $<TARGET_FILE_DIR:nims>)

ADD_CUSTOM_COMMAND(TARGET nims
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/../scripts/nims-init $<TARGET_FILE_DIR:nims>)
