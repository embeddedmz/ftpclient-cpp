cmake_minimum_required(VERSION 2.6)

project(FTPClient)

IF(MSVC OR NOT CMAKE_BUILD_TYPE MATCHES Coverage)

# Locate libcURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

file(GLOB_RECURSE source_files ./*)
add_library(ftpclient STATIC ${source_files})

ENDIF()
