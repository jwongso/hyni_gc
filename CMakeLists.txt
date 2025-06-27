cmake_minimum_required(VERSION 3.20)
project(hyni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0167 OLD)

option(BUILD_TESTING "Build automated tests" ON)

# ===== Dependencies =====
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# ===== CCache Configuration =====
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message("Found ccache ${CCACHE_FOUND}")
    message("Using ccache to speed up compilation")
    set(ENV{CCACHE_CPP2} "yes")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
endif()

# ===== Git Version Info =====
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

if(IS_DIRTY)
    set(HYNI_COMMIT_HASH "${HYNI_COMMIT_HASH}-dirty")
endif()

add_compile_definitions(HYNI_COMMIT_HASH="${HYNI_COMMIT_HASH}")
message(STATUS "Commit hash: ${HYNI_COMMIT_HASH}")

# ===== Architecture Detection =====
function(detect_architecture)
    set(MARCH_NATIVE "")

    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
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
    set(MARCH_NATIVE ${MARCH_NATIVE} PARENT_SCOPE)
endfunction()

detect_architecture()

# ===== Compiler Options =====
# Common flags for all configurations
set(COMMON_FLAGS "-Wall;-Wno-deprecated-declarations;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS")

# Configuration-specific flags
set(RELEASE_FLAGS "-O3;${MARCH_NATIVE};-flto;-fomit-frame-pointer")
set(DEBUG_FLAGS "-O0;-g3")
set(RELWITHDEBINFO_FLAGS "-O3;${MARCH_NATIVE};-g;-flto;-fomit-frame-pointer")

# Apply flags based on configuration and language
add_compile_options(
    "$<$<CONFIG:RELEASE>:${COMMON_FLAGS};${RELEASE_FLAGS}>"
    "$<$<CONFIG:DEBUG>:${COMMON_FLAGS};${DEBUG_FLAGS}>"
    "$<$<CONFIG:RELWITHDEBINFO>:${COMMON_FLAGS};${RELWITHDEBINFO_FLAGS}>"
    "$<$<COMPILE_LANGUAGE:CXX>:-std=c++20>"
)

# ===== Main Library =====
set(HYNI_SOURCES
    src/response_utils.h
    src/config.h
    src/websocket_client.cpp
    src/websocket_client.h
    src/logger.cpp
    src/logger.h
    src/general_context.h
    src/schema_registry.h
    src/general_context.cpp
    src/context_factory.h
    src/http_client.h
    src/http_client.cpp
    src/http_client_factory.h
    src/http_client_factory.cpp
    src/chat_api.h
    src/chat_api.cpp
)

add_library(${PROJECT_NAME} STATIC ${HYNI_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Boost::system
)

# ===== UI Subdirectory =====
add_subdirectory(ui/)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# ===== Testing =====
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    enable_testing()

    # Common test dependencies
    set(TEST_LIBS
        ${PROJECT_NAME}
        GTest::gtest_main
        GTest::gmock
        CURL::libcurl
    )

    # Function to create test executable
    function(add_hyni_test TEST_NAME)
        set(options "")
        set(oneValueArgs "")
        set(multiValueArgs SOURCES)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        add_executable(${TEST_NAME} ${ARG_SOURCES})
        target_link_libraries(${TEST_NAME} ${TEST_LIBS})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    # General tests
    add_hyni_test(${PROJECT_NAME}_TEST
        SOURCES
            tests/response_utils_test.cpp
            tests/websocket_client_test.cpp
            tests/general_context_func_test.cpp
            tests/chat_api_func_test.cpp
            tests/schema_registry_test.cpp
    )

    # Provider-specific tests
    set(PROVIDERS openai claude deepseek mistral)

    foreach(PROVIDER ${PROVIDERS})
        string(TOUPPER ${PROVIDER} PROVIDER_UPPER)
        add_hyni_test(${PROJECT_NAME}_${PROVIDER_UPPER}_TEST
            SOURCES
                tests/${PROVIDER}_schema_test.cpp
                tests/${PROVIDER}_integration_test.cpp
        )
    endforeach()

    # Optional: Create a custom target to run all tests
    add_custom_target(run_all_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS
            ${PROJECT_NAME}_TEST
            ${PROJECT_NAME}_OPENAI_TEST
            ${PROJECT_NAME}_CLAUDE_TEST
            ${PROJECT_NAME}_DEEPSEEK_TEST
            ${PROJECT_NAME}_MISTRAL_TEST
        COMMENT "Running all tests..."
    )

    # Optional: Create targets for specific test categories
    add_custom_target(run_schema_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ".*_schema_test"
        COMMENT "Running schema tests..."
    )

    add_custom_target(run_integration_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ".*_integration_test"
        COMMENT "Running integration tests..."
    )
endif()

# ===== Installation (optional) =====
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.h"
)

# ===== Summary =====
message(STATUS "")
message(STATUS "=== ${PROJECT_NAME} Configuration Summary ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture flags: ${MARCH_NATIVE}")
message(STATUS "Testing enabled: ${BUILD_TESTING}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
