#ifndef INCLUDE_TEST_UTILS_H_
#define INCLUDE_TEST_UTILS_H_

#ifdef WINDOWS
#include <windows.h>
#ifdef _DEBUG
#ifdef _USE_VLD_
#include <vld.h>
#endif
#endif
#endif

#include <sys/stat.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include "SHA1.h"
#include "SimpleIni.h"

bool GlobalTestInit(const std::string& strConfFile);
void GlobalTestCleanUp(void);

void TimeStampTest(std::ostringstream& ssTimestamp);

int TestDLProgressCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);
int TestUPProgressCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);

long GetGMTOffset();
bool GetFileTime(const char* const& pszFilePath, time_t& tLastModificationTime);
long long GetFileSize(const std::string& filename);
long long FdGetFileSize(int fd);

std::string sha1sum(const std::vector<char>& memData);
std::string sha1sum(const std::string& filename);

#endif
