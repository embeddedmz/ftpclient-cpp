# FTP client for C++
[![MIT license](https://img.shields.io/badge/license-MIT-blue.svg)](http://opensource.org/licenses/MIT) ![GitHub Workflow Status](https://img.shields.io/github/workflow/status/embeddedmz/ftpclient-cpp/CMake%20Build%20Matrix) ![img](https://img.shields.io/badge/platform-linux%20%7C%20osx%20%7C%20win-red) ![GitHub all releases](https://img.shields.io/github/downloads/embeddedmz/ftpclient-cpp/total)

## About
This is a simple FTP client for C++. It wraps libcurl for FTP requests and meant to be a portable
and easy-to-use API to perform FTP related operations.

Compilation has been tested with:
- GCC 4.8.5 (Centos 7)
- GCC 5.4.0/7.4.0/9.2.1 (GNU/Linux Ubuntu 16.04/18.04/20.04 LTS)
- Microsoft Visual Studio 2015/2017/2019 (Windows 10)

Underlying libraries:
- [libcurl](http://curl.haxx.se/libcurl/)

Windows Users : vcpkg (Microsoft C++ Library Manager) can be used to easily install libcurl and generate the Visual Studio solution with CMake. With vcpkg, no need to manually copy the DLL in the output directory, vcpkg handles all that ! Look at "Building under Windows via Visual Studio" section, for instructions.

## Usage
Create an object and provide to its constructor a callable object (for log printing) having this signature :

```cpp
void(const std::string&)
```

Later, you can enable log printing by using the flag CFTPClient::SettingsFlag::ENABLE_LOG when initializing a session.

Make sure that Libcurl is compiled with SSH (https://github.com/embeddedmz/ftpclient-cpp/issues/12).

```cpp
#include "FTPClient.h"

CFTPClient FTPClient([](const std::string& strLogMsg){ std::cout << strLogMsg << std::endl; });
```

Before performing one or more requests, a session must be initialized with server parameters (don't prefix the address with an FTP protocol scheme e.g. ftp://).

```cpp
// Classic FTP client
FTPClient.InitSession("127.0.0.1", 21, "username", "password");

// For SFTP : 
SFTPClient.InitSession("127.0.0.1", 22, "username", "password", CFTPClient::FTP_PROTOCOL::SFTP);

/* You might need to set insecure to true to turn off peer/host verification - this is the case if you don't use a CA file */
SFTPClient.SetInsecure(true);
```

With SFTP protocol, all API commands will work except DownloadWildcard.

You can also set parameters such as the time out (in seconds + enable/disable signal handling), the HTTP proxy server etc... before sending your request.

To enable FTP Active Mode, use this setter (I didn't test it but it should work fine, create an issue if it doesn't work) :

```cpp
FTPClient.SetActive(true);
```

To create and remove a remote empty directory :

```cpp
/* creates a directory "bookmarks" under ftp://127.0.0.1:21/documents/ */
FTPClient.CreateDir("/document/bookmarks");

/* removes the "empty" directory "bookmarks" created above
 * the directory must be empty, otherwise the method will fail */
FTPClient.RemoveDir("/document/bookmarks");
```

To download a file :

```cpp
/* download ftp://127.0.0.1:21/info.txt to "C:\downloaded_info.txt" */
FTPClient.DownloadFile("C:\\downloaded_info.txt", "info.txt");
```

To download a whole directory with the wildcard '*' :

```cpp
/* download all the elements of ftp://127.0.0.1:21/pictures/ to WildcardTest/ */
FTPClient.DownloadWildcard("/home/amine/WildcardTest", "pictures/*");
```

To upload and remove a file :

```cpp
/* upload C:\test_upload.txt to  ftp://127.0.0.1:21/upload/documents/test_upload.txt */
/* if /upload/documents/ doesn't exist, you can set the third parameter bCreateDir to true
to create missing directories */
FTPClient.UploadFile("C:\\test_upload.txt", "/upload/documents/test_upload.txt");

/* remove the above uploaded file */
FTPClient.RemoveFile("/upload/documents/test_upload.txt");
```

To list a remote directory:

```cpp
std::string strList;

/* list root directory
 * a third parameter can be set to false to request a detailed list */

FTPClient.List("/", strList);
```

To request a remote file's size and mtime:

```cpp
/* create a helper object to receive file's info */
CFTPClient::FileInfo ResFileInfo = { 0, 0.0 };

/* requests ftp://127.0.0.1:21/info.txt file size and mtime */
FTPClient.Info("info.txt", ResFileInfo));

/* if the operation succeeds, true will be returned. */

cout << ResFileInfo.dFileSize << endl; // file size of "/info.txt"
cout << ResFileInfo.tFileMTime << endl; // file mtime (epoch) of "/info.txt"
```

Always check that the methods above return true, otherwise, that means that  the request wasn't properly
executed.

Finally cleanup can be done automatically if the object goes out of scope. If the log printing is enabled,
a warning message will be printed. Or you can cleanup manually by calling CleanupSession() :

```cpp
FTPClient.CleanupSession();
```

After cleaning the session, if you want to reuse the object, you need to re-initialize it with the
proper method.

## Callback to a Progress Function

A pointer to a callback progress meter function or a callable object (lambda, functor etc...), which should match the prototype shown below, can be passed.

```cpp
int ProgCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);
```

This function gets called by libcurl instead of its internal equivalent with a frequent interval. While data is being transferred it will be called very frequently, and during slow periods like when nothing is being transferred it can slow down to about one call per second.

Returning a non-zero value from this callback will cause libcurl to abort the transfer.

The unit tests "TestDownloadFile" and "TestUploadAndRemoveFile" demonstrate how to use a progress function to display a progress bar on console when downloading or uploading a file.

## Thread Safety

Do not share CFTPClient objects across threads as this would mean accessing libcurl handles from multiple threads at the same time which is not allowed.

The method SetNoSignal can be used to skip all signal handling. This is important in multi-threaded applications as DNS resolution timeouts use signals. The signal handlers quite readily get executed on other threads.

## HTTP Proxy Tunneling Support

An HTTP Proxy can be set to use for the upcoming request.
To specify a port number, append :[port] to the end of the host name. If not specified, `libcurl` will default to using port 1080 for proxies. The proxy string may be prefixed with `http://` or `https://`. If no HTTP(S) scheme is specified, the address provided to `libcurl` will be prefixed with `http://` to specify an HTTP proxy. A proxy host string can embed user + password.
The operation will be tunneled through the proxy as curl option `CURLOPT_HTTPPROXYTUNNEL` is enabled by default.
A numerical IPv6 address must be written within [brackets].

```cpp
FTPClient.SetProxy("https://37.187.100.23:3128");

/* the following request will be tunneled through the proxy */
std::string strList;

FTPClient.List("/", strList);
```

if you need to specify a user and password

```cpp
FTPClient.SetProxyUserPwd("user:password");
```

## Installation

You will need CMake to generate a makefile for the static library or to build the tests/code coverage program.

Also make sure you have libcurl and Google Test installed.

You can follow this script https://gist.github.com/fideloper/f72997d2e2c9fbe66459 to install libcurl.

This tutorial will help you installing properly Google Test on Ubuntu: https://www.eriksmistad.no/getting-started-with-google-test-on-ubuntu/

The CMake script located in the tree will produce Makefiles for the creation of the static library and for the unit tests program.

To create a debug static library and a test binary, change directory to the one containing the first CMakeLists.txt and :

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE:STRING=Debug
make
```

To create a release static library, just change "Debug" by "Release".

The library will be found under "build/[Debug|Release]/lib/libftpclient.a" whereas the test program will be located in "build/[Debug|Release]/bin/test_ftpclient"

To directly run the unit test binary, you must indicate the path of the INI conf file (see the section below)
```Shell
./[Debug|Release]/bin/test_ftpclient /path_to_your_ini_file/conf.ini
```

### Building under Windows via Visual Studio

1. New Procedure (with vcpkg) :

Install [vcpkg](https://github.com/microsoft/vcpkg) then install libcurl (use 'x86-windows' for the 32-bit version) :
```Shell
.\vcpkg install curl curl[openssl] curl[ssh] --triplet=x64-windows
```

If you have a french Visual Studio version, don't forget to install the english language pack (vcpkg will tell you this anyway).

Download and install the latest version of CMake : https://cmake.org/download/ (e.g. Windows win64-x64 Installer)

Open CMake (cmake-gui)

In "Where is the source code", put the ftpclient-cpp path (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/ftpclient-cpp), where the main CMakeLists.txt file exist.

In "Where to build binaries", paste the directory where you want to build the project (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/ftpclient_build)

Click on "Configure".

Select your Visual Studio version (if it isn't already set).
In "Optional platform for generator", you can leave it empty (x64 by default) or choose another value.

Click on the radio button "Specify toolchain file for cross-compiling, then hit the "Next" button.

In "Specify the toolchain file", browse to vcpkg toolchain file (vcpkg/scripts/buildsystems/vcpkg.cmake) and select it.

Press "Finish", wait until CMake configures the project then hit "Generate" to create the Visual Studio solution (library and unit test binary).

2. Old Procedure (without vcpkg) :

First of all, build libcurl using this fork of build-libcurl-windows : https://github.com/ribtoks/build-libcurl-windows

In fact, this fork will allow you to build libcurl under Visual Studio 2017.

```Shell
git clone https://github.com/ribtoks/build-libcurl-windows.git
```

Then just build libcurl using :

```Shell
build.bat
```

This batch script will automatically download the latest libcurl source code and build it using the most recent Visual Studio compiler
that it will find on your computer. For a particular version, you can modify the batch script...

Under YOUR_DIRECTORY\build-libcurl-windows\third-party\libcurl, you will find the curl include directory and a lib directory containing different type of libraries : dynamic and static x86 and x64 libraries compiled in Debug and Release mode (8 libraries). Later, we will be using the libraries located in lib\dll-debug-x64 and lib\dll-release-x64 as an example.

Concerning Google Test, the library will be downloaded and built automatically from its github repository. Someone with enough free time can do the same for libcurl and submit a pull request...

Download and install the latest version of CMake : https://cmake.org/download/ (e.g. Windows win64-x64 Installer)

Open CMake (cmake-gui)

In "Where is the source code", put the ftpclient-cpp path (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/ftpclient-cpp), where the main CMakeLists.txt file exist.

In "Where to build binaries", paste the directory where you want to build the project (e.g. C:/Users/Amine/Documents/Work/PROJECTS/GitHub/ftpclient_build)

Click on "Configure". After some time, an error message will be shown, that's normal as CMake is unable to find libcurl.

Make sure "Advanced" checkbox is checked, in the list, locate the variables prefixed with "CURL_" and update them with the path of libcurl include directory and libraries, for example :

CURL_INCLUDE_DIR C:\LIBS\build-libcurl-windows\third-party\libcurl\include
CURL_LIBRARY_DEBUG C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-debug-x64\libcurl_debug.lib
CURL_LIBRARY_RELEASE C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-release-x64\libcurl.lib

Click on "Configure" again ! You should not have errors this time. You can ignore the warning related to the test configuration file.

Then click on "Generate", you can choose a Visual Studio version if it is not done before (e.g. Visual Studio 15 2017 Win64, I think it should be the same version used to build libcurl...)

Finally, click on "Open Project" to open the solution in Visual Studio.

In Visual Studio, you can change the build type (Debug -> Release). Build the solution (press F7). It must succeed without any errors. You can close Visual Studio.

The library will be found under C:\Users\Amine\Documents\Work\PROJECTS\GitHub\ftpclient_build\lib\Release]\ftpclient.lib

After building a program using "ftpclient.lib", do not forget to copy libcurl DLL in the directory where the program binary is located.

For example, in the build directory (e.g. C:\Users\Amine\Documents\Work\PROJECTS\GitHub\ftpclient_build), under "bin", directory, you may find "Debug", "Release" or both according to the build type used during the build in Visual Studio, and in it, the test program "test_ftpclient.exe". Before executing it, make sure to copy the libcurl DLL in the same directory (e.g. copy C:\LIBS\build-libcurl-windows\third-party\libcurl\lib\dll-release-x64\libcurl.dll and the PDB file too if you want, do not change the name of the DLL !) The type of the library MUST correspond to the type of the .lib file fed to CMake-gui !

If you want to run the test program, in the command line, launch test_ftpclient.exe with the path of you test configuration file (INI) :

```Shell
C:\Users\Amine\Documents\Work\PROJECTS\GitHub\ftpclient_build\bin\[Debug | Release]\test_ftpclient.exe PATH_TO_YOUR_TEST_CONF_FILE\conf.ini
```

## Run Unit Tests

[simpleini](https://github.com/brofield/simpleini) is used to gather unit tests parameters from
an INI configuration file. You need to fill that file with FTP and/or SFTP parameters.
You can also disable some tests (HTTP proxy for instance) and indicate
parameters only for the enabled tests. A template of the INI file already exists under TestFTP/

Example : (Run only FTP tests)

```ini
[tests]
; FTP tests are enabled
ftp=yes
; SFTP tests (not implemented) are disabled
sftp=no
; HTTP Proxy tests are disabled
http-proxy=yes

[ftp]
host=127.0.0.1
port=21
username=foo
password=bar
; remote elements below must exist
remote_file=info.txt
; for the download/info tests
remote_upload_folder=/upload/documents
; for upload/create dir tests
remote_download_folder=pictures/
; for the test TestWildcardedURL to download an entire remote directory
; with the files and directories
```

You can also generate an XML file of test results by adding --getst_output argument when calling the test program

```Shell
./[Debug|Release]/bin/test_ftpclient /path_to_your_ini_file/conf.ini --gtest_output="xml:./TestFTP.xml"
```

An alternative way to compile and run unit tests :

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTEST_INI_FILE="full_or_relative_path_to_your_test_conf.ini"
make
make test
```

You may use a tool like https://github.com/adarmalik/gtest2html to convert your XML test result in an HTML file.

## Memory Leak Check

Visual Leak Detector has been used to check memory leaks with the Windows build (Visual Sutdio 2015)
You can download it here: https://vld.codeplex.com/

To perform a leak check with the Linux build, you can do so :

```Shell
valgrind --leak-check=full ./Debug/bin/test_ftpclient /path_to_ini_file/conf.ini
```

## Code Coverage

The code coverage build doesn't use the static library but compiles and uses directly the 
FTPClient-C++ API in the test program.

```Shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Coverage -DCOVERAGE_INI_FILE:STRING="full_path_to_your_test_conf.ini"
make
make coverage_ftpclient
```

If everything is OK, the results will be found under ./TestFTP/coverage/index.html

Make sure you feed CMake with a full path to your test conf INI file, otherwise, the coverage test
will be useless.

Under Visual Studio, you can simply use OpenCppCoverage (https://opencppcoverage.codeplex.com/)

## Contribute
All contributions are highly appreciated. This includes updating documentation, writing code and unit tests
to increase code coverage and enhance tools.

Try to preserve the existing coding style (Hungarian notation, indentation etc...).

## Issues

### Compiling with the macro DEBUG_CURL

If you compile the test program with the preprocessor macro DEBUG_CURL, to enable curl debug informations,
the static library used must also be compiled with that macro. Don't forget to mention a path where to store
log files in the INI file if you want to use that feature in the unit test program (curl_logs_folder under [local])

### File names format when compiling with Visual Studio (Windows users)

It is assumed that the FTP servers you intend to connect with support UTF-8. You must feed the FTP client API with paths/file names encoded in UTF-8 and NOT in ANSI (Windows-1252 on Western/U.S. systems but it can represent certain other Windows code pages on other systems, ANSI is just an extension for ASCII).

For example, take look at the unit test "TestDownloadFile", the static helper method CFTPClient::AnsiToUtf8 can help you in converting ANSI encoded strings to UTF8.

If you restrict yourself to ASCII characters in your ANSI string, you don't need to convert your ANSI strings to UTF-8.

### Using ftpclient-cpp inside a DLL on Windows

According to this libcurl documentation [page](https://curl.se/libcurl/c/curl_global_init.html) I quote :
"If you are initializing libcurl from a Windows DLL you should not initialize it from DllMain or a static initializer because Windows holds the loader lock during that time and it could cause a deadlock."

So, please avoid to use this library in a DLL or if it's not possible you will have just to made the global libcurl initialization/cleaup stuff yourself (look at CurlHandle class and just call the curl_global_init() once before the beginning of the logic of your program in the main() function and curl_global_cleanup at the end of it and don't forget to remove the instance of that class in the CFTPClient class to fix this issue).
