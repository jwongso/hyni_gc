cmake_minimum_required(VERSION 3.20)
project(hyni_gui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(nlohmann_json REQUIRED)

set(SOURCES
    main.cpp
    main_window.cpp
    chat_widget.cpp
    provider_manager.cpp
    schema_loader.cpp
    api_worker.cpp
    dialogs.cpp
)

set(HEADERS
    main_window.h
    chat_widget.h
    provider_manager.h
    schema_loader.h
    api_worker.h
    dialogs.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${hyni_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    nlohmann_json::nlohmann_json
    hyni
)
