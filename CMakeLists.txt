cmake_minimum_required(VERSION 3.16)
project(toolbox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

add_library(toolboxcore STATIC
  src/core/tool.cpp
  src/core/manifest.cpp
  src/core/http_server.cpp
  src/core/process_manager.cpp
)
target_include_directories(toolboxcore PUBLIC src)
target_link_libraries(toolboxcore PUBLIC Qt6::Core Qt6::Network)

add_executable(toolboxd
  src/cli/main.cpp
)
target_link_libraries(toolboxd PRIVATE toolboxcore)