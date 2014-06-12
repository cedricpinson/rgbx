cmake_minimum_required(VERSION 2.8)
project(rgbx)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/modules)

find_package(OpenImageIO REQUIRED)
include_directories(${OIIO_INCLUDE_DIR})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost COMPONENTS system )

add_executable(rgbx rgbx.cpp)
target_link_libraries(rgbx ${OIIO_LIBRARY} ${Boost_LIBRARIES} )
