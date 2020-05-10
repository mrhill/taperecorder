cmake_minimum_required(VERSION 3.10)

project(taperecorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories (include/tr)
file(GLOB HEADERS "include/tr/*.h")
file(GLOB CPP_SOURCES "src/*.cpp")

add_library(tr ${CPP_SOURCES} ${HEADERS})

#add_subdirectory(apps/hctape)

add_executable(hctape apps/hctape/hctape.cpp)
target_link_libraries(hctape PUBLIC tr)
target_include_directories(hctape PUBLIC include)
