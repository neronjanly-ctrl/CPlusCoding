cmake_minimum_required(VERSION 3.16)

project(CPlusCoding VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(BUILD_CPP_LINUX_LABS "Build the Linux system-programming labs" ON)

if(BUILD_CPP_LINUX_LABS)
    add_subdirectory(cpp-linux-labs)
endif()
