#ifndef INCLUDE_TEST_UTILS_H_
#define INCLUDE_TEST_UTILS_H_

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
#include <streambuf>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <vector>

#ifdef WINDOWS
   #ifdef _DEBUG
      #ifdef _USE_VLD_
      #include <vld.h>
      #endif
   #endif
#endif

#include "SimpleIni.h"

bool GlobalTestInit(const std::string& strConfFile);
void GlobalTestCleanUp(void);

void TimeStampTest(std::ostringstream& ssTimestamp);

int TestDLProgressCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);
int TestUPProgressCallback(void* ptr, double dTotalToDownload, double dNowDownloaded, double dTotalToUpload, double dNowUploaded);

long GetGMTOffset();
bool GetFileTime(const char* const & pszFilePath, time_t& tLastModificationTime);

#endif
