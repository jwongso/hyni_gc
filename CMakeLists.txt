cmake_minimum_required(VERSION 3.20)
project(hyni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0167 OLD)

option(BUILD_TESTING "Build automated tests" ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message("Found ccache ${CCACHE_FOUND}")
    message("Using ccache to speed up compilation")
    set(ENV{CCACHE_CPP2} "yes")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
endif(CCACHE_FOUND)

find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE HYNI_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git diff --quiet --exit-code
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE IS_DIRTY
)

# Append "-dirty" if changes exist
if(IS_DIRTY)
    set(HYNI_COMMIT_HASH "${HYNI_COMMIT_HASH}-dirty")
endif()

add_compile_definitions(HYNI_COMMIT_HASH="${HYNI_COMMIT_HASH}")

message(STATUS "Commit hash: ${HYNI_COMMIT_HASH}")

# Detect CPU architecture and set march/mcpu flags
set(MARCH_NATIVE "")  # Default to empty; fallback to -march=native later

if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    # Detect Apple M1/M2/M3 CPUs
    execute_process(
        COMMAND sysctl -n machdep.cpu.brand_string
        OUTPUT_VARIABLE APPLE_CPU_BRAND
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Detected Apple CPU: ${APPLE_CPU_BRAND}")

    if(APPLE_CPU_BRAND MATCHES "M3")
        set(MARCH_NATIVE "-mcpu=apple-m3")
    elseif(APPLE_CPU_BRAND MATCHES "M2")
        set(MARCH_NATIVE "-mcpu=apple-m2")
    elseif(APPLE_CPU_BRAND MATCHES "M1")
        set(MARCH_NATIVE "-mcpu=apple-m1")
    else()
        message(WARNING "Unknown Apple Silicon CPU, using default flags")
    endif()

elseif(UNIX AND NOT APPLE)
    find_program(LSCPU lscpu)
    if(LSCPU)
        execute_process(
            COMMAND ${LSCPU}
            OUTPUT_VARIABLE LSCPU_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REGEX MATCH "Model name:[^\n]+" CPU_LINE "${LSCPU_OUTPUT}")
        string(REGEX REPLACE ".*Model name:[ \t]*" "" CPU_MODEL "${CPU_LINE}")

        message(STATUS "Detected CPU model: ${CPU_MODEL}")

        if(CPU_MODEL MATCHES "Zen 2")
            set(MARCH_NATIVE "-march=znver2")
        elseif(CPU_MODEL MATCHES "Zen 3")
            set(MARCH_NATIVE "-march=znver3")
        elseif(CPU_MODEL MATCHES "Zen 4")
            set(MARCH_NATIVE "-march=znver4")
        elseif(CPU_MODEL MATCHES "Alder Lake")
            set(MARCH_NATIVE "-march=alderlake")
        elseif(CPU_MODEL MATCHES "Ryzen AI 9 HX 370")
            set(MARCH_NATIVE "-march=znver5")
        endif()
    endif()
endif()

if(MARCH_NATIVE STREQUAL "")
    set(MARCH_NATIVE "-march=native")
endif()

message(STATUS "Using architecture-specific flags: ${MARCH_NATIVE}")

string(
    APPEND opts
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:CXX>>:-Wall;-Wno-deprecated-declarations;-O3;${MARCH_NATIVE};-std=c++20;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-flto;-fomit-frame-pointer>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:CXX>>:-Wall;-Wno-deprecated-declarations;-O0;-g3;-std=c++20;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:CXX>>:-Wall;-Wno-deprecated-declarations;${MARCH_NATIVE};-O3;-g;-std=c++20;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-flto;-fomit-frame-pointer>"
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:C>>:-Wall;-Wno-deprecated-declarations;${MARCH_NATIVE};-O3;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-flto;-fomit-frame-pointer>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:C>>:-Wall;-Wno-deprecated-declarations;-O0;-g3;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:C>>:-Wall;-Wno-deprecated-declarations;${MARCH_NATIVE};-O3;-g;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-flto;-fomit-frame-pointer>"
)
add_compile_options("${opts}")

add_library(${PROJECT_NAME} SHARED
    src/model_context.h
    src/openai_context.h
    src/openai_context.cpp
    src/deepseek_context.h
    src/deepseek_context.cpp
    src/claudeai_context.h
    src/claudeai_context.cpp
    src/response_utils.h
    src/chat_api.cpp
    src/chat_api.h
    src/config.h
    src/websocket_client.cpp
    src/websocket_client.h
    src/logger.cpp
    src/logger.h
    src/general_context.h
    src/schema_manager.h
    src/general_context.cpp
    src/schema_manager.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    curl
    nlohmann_json::nlohmann_json
    Boost::system
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if (BUILD_TESTING)
find_package(GTest REQUIRED)
enable_testing()

add_executable(${PROJECT_NAME}_TEST
    tests/chat_api_test.cpp
    tests/response_utils_test.cpp
    tests/websocket_client_test.cpp
    tests/general_context_func_test.cpp
)

target_link_libraries(${PROJECT_NAME}_TEST
    ${PROJECT_NAME}
    GTest::gtest
    GTest::gmock
    curl
)

add_test(NAME    ${PROJECT_NAME}_TEST
         COMMAND ${PROJECT_NAME}_TEST)

endif(BUILD_TESTING)
