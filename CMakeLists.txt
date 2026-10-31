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

add_executable(process_demo src/process_demo.cpp)

target_compile_options(process_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(exec_demo src/exec_demo.cpp)

target_compile_options(exec_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(pipe_demo src/pipe_demo.cpp)

target_compile_options(pipe_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(pipe_exec_demo src/pipe_exec_demo.cpp)

target_compile_options(pipe_exec_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(pipeline_demo src/pipeline_demo.cpp)

target_compile_options(pipeline_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

find_package(Threads REQUIRED)

add_executable(producer_consumer_demo
    src/producer_consumer_demo.cpp
)

target_compile_options(producer_consumer_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(producer_consumer_demo PRIVATE
    Threads::Threads
)

add_executable(race_demo
    src/race_demo.cpp
)

target_compile_options(race_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(race_demo PRIVATE
    Threads::Threads
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

option(
    ENABLE_TSAN
    "Enable ThreadSanitizer"
    OFF
)

if(ENABLE_TSAN AND ENABLE_SANITIZERS)
    message(FATAL_ERROR
        "TSan cannot be combined with ASan/UBSan"
    )
endif()

if(ENABLE_TSAN)
    target_compile_options(race_demo PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
        -g
    )

    target_link_options(race_demo PRIVATE
        -fsanitize=thread
    )
endif()

enable_testing()
add_test(NAME file_io_demo COMMAND file_io_demo)
# The teaching demo uses fixed /tmp paths; do not run concurrent copies.
set_tests_properties(file_io_demo PROPERTIES RUN_SERIAL TRUE)
