
IF(MSVC OR NOT CMAKE_BUILD_TYPE MATCHES Coverage)

#Locate libcURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

file(GLOB_RECURSE source_files ./*)
add_library(ftpclient ${source_files})

ENDIF()
