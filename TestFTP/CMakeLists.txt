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

# For Windows
# https://crascit.com/2015/07/25/cmake-gtest/
if (MSVC)
	# Download and unpack googletest at configure time
	configure_file(CMakeLists.txt.in "${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt")
	execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )
	execute_process(COMMAND "${CMAKE_COMMAND}" --build .
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )

	# Prevent GoogleTest from overriding our compiler/linker options
	# when building with Visual Studio
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	# Add googletest directly to our build. This adds
	# the following targets: gtest, gtest_main, gmock
	# and gmock_main
	add_subdirectory("${CMAKE_BINARY_DIR}/googletest-src"
					 "${CMAKE_BINARY_DIR}/googletest-build")

	# The gtest/gmock targets carry header search path
	# dependencies automatically when using CMake 2.8.11 or
	# later. Otherwise we have to add them here ourselves.
	if(CMAKE_VERSION VERSION_LESS 2.8.11)
		include_directories("${gtest_SOURCE_DIR}/include"
							"${gmock_SOURCE_DIR}/include")
	endif()

	# Now simply link your own targets against gtest, gmock,
	# etc. as appropriate
endif()

# Locate GTest
if(NOT MSVC)
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
endif()

include_directories(../FTP)
include_directories(./simpleini)
include_directories(./)

IF(NOT MSVC AND CMAKE_BUILD_TYPE MATCHES Coverage)

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

ELSE()

#link_directories(${CMAKE_BINARY_DIR}/lib)

#Output Setup
add_executable(test_ftpclient main.cpp test_utils.cpp)

#Link setup
if(NOT MSVC)
	target_link_libraries(test_ftpclient ftpclient ${GTEST_LIBRARIES} pthread curl)
else()
	target_link_libraries(test_ftpclient ftpclient gtest ${CURL_LIBRARIES})
endif()

ENDIF()
