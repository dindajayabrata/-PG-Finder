cmake_minimum_required(VERSION 3.20)
project(pg_finder_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ODBC REQUIRED)

add_executable(pg_finder_backend src/server.cpp)
target_link_libraries(pg_finder_backend PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    ODBC::ODBC
)

if (WIN32)
    target_link_libraries(pg_finder_backend PRIVATE ws2_32)
endif()
