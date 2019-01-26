cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib)

if(NOT MSVC)
	add_definitions(-DLINUX)
else()
	add_definitions(-DWINDOWS)
endif()

include_directories(FTP)

add_subdirectory(FTP)
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
