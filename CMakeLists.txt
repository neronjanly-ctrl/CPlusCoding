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

add_executable(file_io_demo src/file_io_demo.cpp)

target_compile_options(file_io_demo PRIVATE
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
    foreach(lab_target IN ITEMS hello file_io_demo)
        target_compile_options(${lab_target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${lab_target} PRIVATE
            -fsanitize=address,undefined
        )
    endforeach()
endif()

enable_testing()
add_test(NAME file_io_demo COMMAND file_io_demo)
# The teaching demo uses fixed /tmp paths; do not run concurrent copies.
set_tests_properties(file_io_demo PROPERTIES RUN_SERIAL TRUE)
