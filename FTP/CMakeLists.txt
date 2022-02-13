
IF(MSVC OR NOT CMAKE_BUILD_TYPE MATCHES Coverage)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#Locate libcURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

file(GLOB_RECURSE source_files ./*)
add_library(ftpclient STATIC ${source_files})

install(TARGETS ftpclient)

ENDIF()
