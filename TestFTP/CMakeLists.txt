cmake_minimum_required(VERSION 2.6)

project(TestFTPClient)

# Code coverage setup
IF(CMAKE_BUILD_TYPE MATCHES Coverage)
   INCLUDE(CodeCoverage.cmake)
   SET(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
   SET(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
ENDIF(CMAKE_BUILD_TYPE MATCHES Coverage)

# Locate libcURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

include_directories(../FTP)
include_directories(./simpleini)
include_directories(./)

IF(CMAKE_BUILD_TYPE MATCHES Coverage)

file(GLOB_RECURSE ftp_source_files ../FTP/*)

#Output Setup
add_executable(test_ftpclient main.cpp test_utils.cpp ${ftp_source_files})

#Link setup
target_link_libraries(test_ftpclient ${GTEST_LIBRARIES} pthread curl)

SETUP_TARGET_FOR_COVERAGE(
           coverage_ftpclient   # Name for custom target.
           test_ftpclient       # Name of the test driver executable that runs the tests.
                                # NOTE! This should always have a ZERO as exit code
                                # otherwise the coverage generation will not complete.
           coverage             # Name of output directory.
           ${COVERAGE_INI_FILE} # Optional fourth parameter is passed as arguments to _testrunner
                                # Pass them in list form, e.g.: "-j;2" for -j 2
           )

ELSE(CMAKE_BUILD_TYPE MATCHES Coverage)

#link_directories(${CMAKE_BINARY_DIR}/lib)

#Output Setup
add_executable(test_ftpclient main.cpp test_utils.cpp)

#Link setup
target_link_libraries(test_ftpclient ftpclient ${GTEST_LIBRARIES} pthread curl)

ENDIF(CMAKE_BUILD_TYPE MATCHES Coverage)
