cmake_minimum_required(VERSION 3.16)

project(cpp_linux_labs VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

option(
    ENABLE_TSAN
    "Enable ThreadSanitizer for the safe/unsafe race comparison"
    OFF
)

option(
    BUILD_UNSAFE_LABS
    "Build opt-in demonstrations that intentionally contain undefined behavior"
    OFF
)

if(ENABLE_TSAN AND ENABLE_SANITIZERS)
    message(FATAL_ERROR "TSan cannot be combined with ASan/UBSan")
endif()

if(ENABLE_TSAN AND NOT BUILD_UNSAFE_LABS)
    message(FATAL_ERROR "ENABLE_TSAN requires BUILD_UNSAFE_LABS=ON")
endif()

if(ENABLE_TSAN)
    find_program(SETARCH_EXECUTABLE setarch REQUIRED)
endif()

find_package(Threads REQUIRED)

function(add_lab_executable target source)
    add_executable(${target} ${source})
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    if(ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()

add_lab_executable(hello src/hello.cpp)
add_lab_executable(file_io_demo src/file_io_demo.cpp)
add_lab_executable(process_demo src/process_demo.cpp)
add_lab_executable(exec_demo src/exec_demo.cpp)
add_lab_executable(pipe_demo src/pipe_demo.cpp)
add_lab_executable(pipe_exec_demo src/pipe_exec_demo.cpp)
add_lab_executable(pipeline_demo src/pipeline_demo.cpp)
add_lab_executable(producer_consumer_demo src/producer_consumer_demo.cpp)
add_lab_executable(race_demo src/race_demo.cpp)
add_lab_executable(mmap_demo src/mmap_demo.cpp)

target_link_libraries(producer_consumer_demo PRIVATE Threads::Threads)
target_link_libraries(race_demo PRIVATE Threads::Threads)
target_link_libraries(mmap_demo PRIVATE Threads::Threads)

if(BUILD_UNSAFE_LABS)
    add_lab_executable(race_unsafe_demo src/race_unsafe_demo.cpp)
    target_link_libraries(race_unsafe_demo PRIVATE Threads::Threads)
endif()

if(ENABLE_TSAN)
    foreach(race_target IN ITEMS race_demo race_unsafe_demo)
        target_compile_options(${race_target} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(${race_target} PRIVATE -fsanitize=thread)
    endforeach()
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME d1_file_io COMMAND file_io_demo)
    set_tests_properties(d1_file_io PROPERTIES
        LABELS "D1"
        RUN_SERIAL TRUE
        PASS_REGULAR_EXPRESSION "PASS: errno experiment"
    )

    add_test(NAME d2_process COMMAND process_demo)
    add_test(NAME d2_exec COMMAND exec_demo)
    add_test(NAME d2_pipe COMMAND pipe_demo)
    add_test(NAME d2_pipe_exec COMMAND pipe_exec_demo)
    add_test(NAME d2_pipeline COMMAND pipeline_demo)
    set_tests_properties(
        d2_process d2_exec d2_pipe d2_pipe_exec d2_pipeline
        PROPERTIES LABELS "D2"
    )

    add_test(NAME d3_producer_consumer COMMAND producer_consumer_demo)
    if(ENABLE_TSAN)
        # WSL's default address layout can collide with GCC TSan shadow memory.
        # Disable ASLR for this process only; no global sysctl is changed.
        add_test(
            NAME d3_race_fixed
            COMMAND ${SETARCH_EXECUTABLE} ${CMAKE_SYSTEM_PROCESSOR}
                    -R $<TARGET_FILE:race_demo>
        )
    else()
        add_test(NAME d3_race_fixed COMMAND race_demo)
    endif()
    set_tests_properties(
        d3_producer_consumer d3_race_fixed
        PROPERTIES LABELS "D3"
    )

    add_test(NAME d4_mmap COMMAND mmap_demo)
    set_tests_properties(d4_mmap PROPERTIES LABELS "D4")
endif()
