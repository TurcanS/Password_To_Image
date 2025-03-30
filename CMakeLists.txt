cmake_minimum_required(VERSION 3.16)
project(enc_dec VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)  # Handle Qt Meta-Object Compiler
set(CMAKE_AUTORCC ON)  # Handle Qt Resource Compiler
set(CMAKE_AUTOUIC ON)  # Handle Qt User Interface Compiler

# Find Qt packages
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# UI files
set(UI_FILES
    # Add your .ui files here if needed
    # src/mainwindow.ui
)

# Resource files
set(RESOURCE_FILES
    # Add your .qrc files here if needed
    # resources/resources.qrc
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/crypto_utils.cpp
    src/image_utils.cpp
    Include/lodepng.cpp
)

set(HEADERS
    src/mainwindow.h
    src/crypto_utils.h
    src/image_utils.h
    Include/lodepng.h
)

# Create executable
add_executable(${PROJECT_NAME} 
    ${SOURCES} 
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCE_FILES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Widgets
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Include directories properly
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

# Install rules
install(TARGETS ${PROJECT_NAME} DESTINATION bin)