cmake_minimum_required(VERSION 3.16)

project(cpp_linux_labs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hello src/hello.cpp)

target_compile_options(hello PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

option(
    ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

if(ENABLE_SANITIZERS)
    target_compile_options(hello PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )

    target_link_options(hello PRIVATE
        -fsanitize=address,undefined
    )
endif()
