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

# --------------------------------------------------
# Common executable configuration
# --------------------------------------------------

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

# --------------------------------------------------
# D1: File I/O
# --------------------------------------------------

add_lab_executable(hello src/hello.cpp)
add_lab_executable(file_io_demo src/file_io_demo.cpp)

# --------------------------------------------------
# D2: Linux Process Management
# --------------------------------------------------

add_lab_executable(process_demo src/process_demo.cpp)
add_lab_executable(exec_demo src/exec_demo.cpp)
add_lab_executable(pipe_demo src/pipe_demo.cpp)
add_lab_executable(pipe_exec_demo src/pipe_exec_demo.cpp)
add_lab_executable(pipeline_demo src/pipeline_demo.cpp)

# --------------------------------------------------
# D3: Threads and Synchronization
# --------------------------------------------------

add_lab_executable(producer_consumer_demo src/producer_consumer_demo.cpp)
add_lab_executable(race_demo src/race_demo.cpp)

target_link_libraries(producer_consumer_demo PRIVATE Threads::Threads)
target_link_libraries(race_demo PRIVATE Threads::Threads)

# --------------------------------------------------
# D4: Shared Memory and Signals
# --------------------------------------------------

add_lab_executable(mmap_demo src/mmap_demo.cpp)
add_lab_executable(signal_demo src/signal_demo.cpp)
add_lab_executable(pipe_duplex_demo src/pipe_duplex_demo.cpp)
add_lab_executable(socketpair_demo src/socketpair_demo.cpp)

target_link_libraries(mmap_demo PRIVATE Threads::Threads)

# --------------------------------------------------
# D5: TCP Networking
# --------------------------------------------------

add_lab_executable(tcp_echo_server src/tcp_echo_server.cpp)
add_lab_executable(tcp_echo_client src/tcp_echo_client.cpp)
add_lab_executable(tcp_protocol_test tests/tcp_protocol_test.cpp)
add_lab_executable(tcp_framed_server src/tcp_framed_server.cpp)
add_lab_executable(tcp_framed_client src/tcp_framed_client.cpp)

target_link_libraries(
    tcp_framed_server
    PRIVATE Threads::Threads
)

target_link_libraries(
    tcp_protocol_test
    PRIVATE Threads::Threads
)

target_include_directories(
    tcp_protocol_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --------------------------------------------------
# Optional unsafe demonstrations
# --------------------------------------------------

if(BUILD_UNSAFE_LABS)
    add_lab_executable(race_unsafe_demo src/race_unsafe_demo.cpp)
    target_link_libraries(race_unsafe_demo PRIVATE Threads::Threads)
endif()

# --------------------------------------------------
# ThreadSanitizer configuration
# --------------------------------------------------

if(ENABLE_TSAN)
    foreach(race_target IN ITEMS race_demo race_unsafe_demo)
        target_compile_options(${race_target} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )

        target_link_options(${race_target} PRIVATE
            -fsanitize=thread
        )
    endforeach()
endif()

# --------------------------------------------------
# CTest configuration
# --------------------------------------------------

include(CTest)

if(BUILD_TESTING)

    # D1
    add_test(NAME d1_file_io COMMAND file_io_demo)

    set_tests_properties(d1_file_io PROPERTIES
        LABELS "D1"
        RUN_SERIAL TRUE
        PASS_REGULAR_EXPRESSION "PASS: errno experiment"
    )

    # D2
    add_test(NAME d2_process COMMAND process_demo)
    add_test(NAME d2_exec COMMAND exec_demo)
    add_test(NAME d2_pipe COMMAND pipe_demo)
    add_test(NAME d2_pipe_exec COMMAND pipe_exec_demo)
    add_test(NAME d2_pipeline COMMAND pipeline_demo)

    set_tests_properties(
        d2_process
        d2_exec
        d2_pipe
        d2_pipe_exec
        d2_pipeline
        PROPERTIES LABELS "D2"
    )

    # D3
    add_test(NAME d3_producer_consumer COMMAND producer_consumer_demo)

    if(ENABLE_TSAN)
        # WSL's default address layout can collide with GCC TSan shadow memory.
        # Disable ASLR for this process only.
        add_test(
            NAME d3_race_fixed
            COMMAND ${SETARCH_EXECUTABLE} ${CMAKE_SYSTEM_PROCESSOR}
                    -R $<TARGET_FILE:race_demo>
        )
    else()
        add_test(NAME d3_race_fixed COMMAND race_demo)
    endif()

    set_tests_properties(
        d3_producer_consumer
        d3_race_fixed
        PROPERTIES LABELS "D3"
    )

    # D4
    add_test(NAME d4_mmap COMMAND mmap_demo)
    add_test(NAME d4_signal COMMAND signal_demo)
    add_test(NAME d4_pipe_duplex COMMAND pipe_duplex_demo)
    add_test(NAME d4_socketpair COMMAND socketpair_demo)

    set_tests_properties(
    d4_mmap
    d4_signal
    d4_pipe_duplex
    d4_socketpair
    PROPERTIES LABELS "D4"
)

set_tests_properties(d4_signal PROPERTIES
    PASS_REGULAR_EXPRESSION "PASS: signal delivery and child cleanup"
)

set_tests_properties(d4_pipe_duplex PROPERTIES
    PASS_REGULAR_EXPRESSION "PASS: duplex pipe communication"
)

set_tests_properties(d4_socketpair PROPERTIES
    PASS_REGULAR_EXPRESSION "PASS: socketpair communication"
)

    # D5
    add_test(NAME d5_tcp_protocol COMMAND tcp_protocol_test)

    set_tests_properties(d5_tcp_protocol PROPERTIES
        LABELS "D5"
        PASS_REGULAR_EXPRESSION "PASS: tcp protocol framing"
        TIMEOUT 10
    )

endif()
