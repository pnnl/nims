cmake_minimum_required (VERSION 2.6) 
project (NIMS) 

#find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc video)
find_package(Boost REQUIRED COMPONENTS filesystem system program_options)

#include_directories(${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(ingester ingester.cpp)
add_executable(detector detector.cpp)
add_executable(tracker tracker.cpp)
add_executable(nims nims.cpp)

#target_link_libraries(ingester ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(ingester ${Boost_LIBRARIES})
#target_link_libraries(detector ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(detector ${Boost_LIBRARIES})
#target_link_libraries(tracker ${OpenCV_LIBS} ${Boost_LIBRARIES})
target_link_libraries(tracker ${Boost_LIBRARIES})
target_link_libraries(nims ${Boost_LIBRARIES})

