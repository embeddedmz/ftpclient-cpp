cmake_minimum_required (VERSION 3.10.0)

# Read version from file VERSION
FILE(READ "VERSION" project_VERSION)
STRING(STRIP "${project_VERSION}" project_VERSION)

SET(LICENSE "MIT")
cmake_policy(SET CMP0022 NEW)

PROJECT(FTPClient VERSION ${project_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)

if(NOT MSVC)
	add_definitions(-DLINUX)
else()
	add_definitions(-DWINDOWS)
endif()

option(SKIP_TESTS_BUILD "Skip tests build" ON)

include_directories(FTP)

add_subdirectory(FTP)

if(NOT SKIP_TESTS_BUILD)
add_subdirectory(TestFTP)

include(CTest)
enable_testing ()

# test if the test INI file exist, otherwise default it to the one in TestFTP folder
IF (NOT TEST_INI_FILE)
    SET(TEST_INI_FILE "./TestFTP/template_test_conf.ini")
    MESSAGE(WARNING "You didn't provide an INI test configuration file.\
    Defaulting TEST_INI_FILE to ./TestFTP/template_test_conf.ini")
ENDIF()

add_test (NAME FtpClientTest COMMAND test_ftpclient ${TEST_INI_FILE})
endif(NOT SKIP_TESTS_BUILD)
