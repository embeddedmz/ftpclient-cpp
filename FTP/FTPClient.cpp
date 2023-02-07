/**
 * @file FTPClient.cpp
 * @brief implementation of the FTP client class
 * @author Mohamed Amine Mzoughi <mohamed-amine.mzoughi@laposte.net>
 */

#include "FTPClient.h"

#include <iterator>
#include <stdexcept>

#define UNUSED(x) static_cast<void>(x);

namespace embeddedmz {

// Static members initialization

#ifdef DEBUG_CURL
std::string CFTPClient::s_strCurlTraceLogDirectory;
#endif

/**
 * @brief constructor of the FTP client object
 *
 * @param Logger - a callabck to a logger function void(const std::string&)
 *
 */
CFTPClient::CFTPClient(LogFnCallback Logger)
    :
      m_bActive(false),
      m_bNoSignal(true),
      m_bInsecure(false),
      m_uPort(0),
      m_eFtpProtocol(FTP_PROTOCOL::FTP),
      m_eSettingsFlags(NO_FLAGS),
      m_pCurlSession(nullptr),
      m_iCurlTimeout(0),
      m_bProgressCallbackSet(false),
      m_oLog(std::move(Logger)),
      m_curlHandle(CurlHandle::instance())
{
   if (!m_oLog) {
      throw std::logic_error{"Invalid logger clb applied"};
   }
}



/**
 * @brief destructor of the FTP client object
 *
 */
CFTPClient::~CFTPClient() {
   if (m_pCurlSession != nullptr) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_WARNING_OBJECT_NOT_CLEANED);

      CleanupSession();
   }
}

/**
 * @brief starts a new FTP session, initializes the cURL API session
 *
 * if a new session was already started, the method has no effect.
 *
 * @param [in] strHost server address
 * @param [in] uPort the remote port
 * @param [in] strLogin username
 * @param [in] strPassword password
 * @param [in] eFtpProtocol the protocol used to connect to the server (FTP,
 * FTPS, FTPES, SFTP)
 * @param [in] eSettingsFlags optional use | operator to choose multiple options
 *
 * @retval true   Successfully initialized the session.
 * @retval false  The session is already initialized.
 * call CleanupSession() before initializing a new one or the Curl API is not
 * initialized.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->InitSession("ftp://127.0.0.1", 21, "username", "password");
 * @endcode
 */
bool CFTPClient::InitSession(const std::string &strHost, const unsigned &uPort, const std::string &strLogin,
                                   const std::string &strPassword, const FTP_PROTOCOL &eFtpProtocol /* = FTP */,
                                   const SettingsFlag &eSettingsFlags /* = NO_FLAGS */) {
   if (strHost.empty()) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_EMPTY_HOST_MSG);

      return false;
   }

   if (m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_ALREADY_INIT_MSG);

      return false;
   }
   m_pCurlSession = curl_easy_init();

   m_strServer      = strHost;
   m_uPort          = uPort;
   m_strUserName    = strLogin;
   m_strPassword    = strPassword;
   m_eFtpProtocol   = eFtpProtocol;
   m_eSettingsFlags = eSettingsFlags;

   return (m_pCurlSession != nullptr);
}

/**
 * @brief cleans the current FTP session
 *
 * if a session was not already started, the method has no effect
 *
 * @retval true   Successfully cleaned the current session.
 * @retval false  The session is not initialized.
 *
 * Example Usage:
 * @code
 *    objFTPClient.CleanupSession();
 * @endcode
 */
bool CFTPClient::CleanupSession() {
   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }

#ifdef DEBUG_CURL
   if (m_ofFileCurlTrace.is_open()) {
      m_ofFileCurlTrace.close();
   }
#endif

   curl_easy_cleanup(m_pCurlSession);
   m_pCurlSession = nullptr;

   return true;
}

/**
 * @brief sets the progress function callback and the owner of the client
 *
 * @param [in] pOwner pointer to the object owning the client, nullptr otherwise
 * @param [in] fnCallback callback to progress function
 *
 */
void CFTPClient::SetProgressFnCallback(void *pOwner, const ProgressFnCallback &fnCallback, const bool enable /*= true*/) {
   m_ProgressStruct.pOwner       = pOwner;
   m_fnProgressCallback          = fnCallback;
   m_ProgressStruct.pCurl        = m_pCurlSession;
   m_ProgressStruct.dLastRunTime = 0;
   m_bProgressCallbackSet        = enable;
}

/**
 * @brief sets the HTTP Proxy address to tunnel the operation through it
 *
 * @param [in] strProxy URI of the HTTP Proxy
 *
 */
void CFTPClient::SetProxy(const std::string &strProxy) {
   if (strProxy.empty()) return;

   std::string strUri = strProxy;
   std::transform(strUri.begin(), strUri.end(), strUri.begin(), ::toupper);

   if (strUri.compare(0, 5, "HTTP:") != 0)
      m_strProxy = "http://" + strProxy;
   else
      m_strProxy = strProxy;
};

 /**
 * @brief sets the HTTP Proxy user and password
 *
 * @param [in] strProxyUserPwd string of the HTTP Proxy user:password
 *
 */
void CFTPClient::SetProxyUserPwd(const std::string &strProxyUserPwd) {
   m_strProxyUserPwd = strProxyUserPwd;
};
    
/**
 * @brief generates a URI
 *
 * '/' will be duplicated to be interpreted correctly by the Curl API.
 *
 * @param [in] strRemoteFile URL of the file.
 *
 * @retval std::string A complete URI containing the requested resource.
 *
 * Example Usage:
 * @code
 *    ParseURL("documents/info.txt"); //returns
 * "ftp://127.0.0.1//documents//info.txt"
 * @endcode
 */
std::string CFTPClient::ParseURL(const std::string &strRemoteFile) const {
   std::string strURL = m_strServer + "/" + strRemoteFile;

   ReplaceString(strURL, "/", "//");
   ReplaceString(strURL, " ", "%20"); //fixes folders with spaces not working

   std::string strUri = strURL;

   // boost::to_upper(strUri);
   std::transform(strUri.begin(), strUri.end(), strUri.begin(), ::toupper);

   if (strUri.compare(0, 4, "FTP:") != 0 && strUri.compare(0, 5, "SFTP:") != 0) {
      switch (m_eFtpProtocol) {
         case FTP_PROTOCOL::FTP:
         case FTP_PROTOCOL::FTPES:
         default:
            strURL = "ftp://" + strURL;
            break;

         case FTP_PROTOCOL::FTPS:
            strURL = "ftps://" + strURL;
            break;

         case FTP_PROTOCOL::SFTP:
            strURL = "sftp://" + strURL;
            break;
      }
   }

   return strURL;
}

/**
 * @brief creates a remote directory
 *
 * @param [in] strNewDir the remote direcotry to be created encoded in UTF-8 format.
 *
 * @retval true   Successfully created a directory.
 * @retval false  The directory couldn't be created.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->CreateDir("upload/bookmarks");
 *    // Will create bookmarks directory under the upload dir (must exist).
 * @endcode
 */
bool CFTPClient::CreateDir(const std::string &strNewDir) const {
   if (strNewDir.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   struct curl_slist *headerlist = nullptr;

   std::string strRemoteFolder;
   std::string strRemoteNewFolderName;
   std::string strBuf;
   bool bRet = false;

   if (m_eFtpProtocol == FTP_PROTOCOL::SFTP) {
      strRemoteFolder        = ParseURL("");
      strRemoteNewFolderName = strNewDir;

      // Append the rmdir command
      strBuf += "mkdir ";
   } else {
      // Splitting folder name
      std::size_t uFound = strNewDir.find_last_of("/");
      if (uFound != std::string::npos) {
         strRemoteFolder        = ParseURL(strNewDir.substr(0, uFound)) + "//";
         strRemoteNewFolderName = strNewDir.substr(uFound + 1);
      } else  // the dir. to be created is located in the root directory
      {
         strRemoteFolder        = ParseURL("");
         strRemoteNewFolderName = strNewDir;
      }
      // Append the MKD command
      strBuf += "MKD ";
   }

   // Specify target
   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strRemoteFolder.c_str());

   strBuf += strRemoteNewFolderName;
   headerlist = curl_slist_append(headerlist, strBuf.c_str());

   curl_easy_setopt(m_pCurlSession, CURLOPT_POSTQUOTE, headerlist);
   curl_easy_setopt(m_pCurlSession, CURLOPT_NOBODY, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADER, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR);
   
   /* enable TCP keep-alive for this transfer */
   curl_easy_setopt(m_pCurlSession, CURLOPT_TCP_KEEPALIVE, 0L);

   CURLcode res = Perform();

   // Check for errors
   if (res != CURLE_OK) {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_MKDIR_FORMAT, strRemoteNewFolderName.c_str(), res, curl_easy_strerror(res)));
   } else
      bRet = true;

   // clean up the FTP commands list
   curl_slist_free_all(headerlist);

   return bRet;
}

/**
 * @brief removes an empty remote directory
 *
 * if the remote directory ain't empty, the method will fail.
 * yhe user must use RemoveFile() on all directory's files then use RemoveDir to
 * delete the latter.
 *
 * @param [in] strDir the remote direcotry to be removed encoded in UTF-8 format.
 *
 * @retval true   Successfully removed a directory.
 * @retval false  The directory couldn't be removed.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->RemoveDir("upload/bookmarks");
 *    // Will remove bookmarks directory, upload must exist.
 * @endcode
 */
bool CFTPClient::RemoveDir(const std::string &strDir) const {
   if (strDir.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   struct curl_slist *headerlist = nullptr;

   std::string strRemoteFolder;
   std::string strRemoteFolderName;
   std::string strBuf;
   bool bRet = false;

   if (m_eFtpProtocol == FTP_PROTOCOL::SFTP) {
      strRemoteFolder     = ParseURL("");
      strRemoteFolderName = strDir;

      // Append the rmdir command
      strBuf += "rmdir ";
   } else {
      // Splitting folder name
      std::size_t uFound = strDir.find_last_of("/");
      if (uFound != std::string::npos) {
         strRemoteFolder     = ParseURL(strDir.substr(0, uFound)) + "//";
         strRemoteFolderName = strDir.substr(uFound + 1);
      } else  // the dir. to be removed is located in the root directory
      {
         strRemoteFolder     = ParseURL("");
         strRemoteFolderName = strDir;
      }

      // Append the rmd command
      strBuf += "RMD ";
   }

   // Specify target
   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strRemoteFolder.c_str());
   
   strBuf += strRemoteFolderName;
   headerlist = curl_slist_append(headerlist, strBuf.c_str());

   curl_easy_setopt(m_pCurlSession, CURLOPT_POSTQUOTE, headerlist);
   curl_easy_setopt(m_pCurlSession, CURLOPT_NOBODY, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADER, 1L);

   CURLcode res = Perform();

   if (res != CURLE_OK) {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_RMDIR_FORMAT, strRemoteFolderName.c_str(), res, curl_easy_strerror(res)));
   } else
      bRet = true;

   // clean up the FTP commands list
   curl_slist_free_all(headerlist);

   return bRet;
}

/**
 * @brief deletes a remote file
 *
 * @param [in] strRemoteFile the URL of the remote file encoded in UTF-8 format.
 *
 * @retval true   Successfully deleted the file.
 * @retval false  The file couldn't be deleted.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->RemoveFile("documents/Config.txt");
 *    // e.g. : Will delete ftp://127.0.0.1/documents/Config.txt
 * @endcode
 */
bool CFTPClient::RemoveFile(const std::string &strRemoteFile) const {
   if (strRemoteFile.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   struct curl_slist *headerlist = nullptr;

   std::string strRemoteFolder;
   std::string strRemoteFileName;
   std::string strBuf;

   bool bRet = false;

   if (m_eFtpProtocol == FTP_PROTOCOL::SFTP) {
      strRemoteFolder   = ParseURL("");
      strRemoteFileName = strRemoteFile;

      // Append the rm command
      strBuf += "rm ";
   } else {
      // Splitting file name
      std::size_t uFound = strRemoteFile.find_last_of("/");
      if (uFound != std::string::npos) {
         strRemoteFolder   = ParseURL(strRemoteFile.substr(0, uFound)) + "//";
         strRemoteFileName = strRemoteFile.substr(uFound + 1);
      } else  // the file to be deleted is located in the root directory
      {
         strRemoteFolder   = ParseURL("");
         strRemoteFileName = strRemoteFile;
      }
      
      // Append the delete command
      strBuf += "DELE ";
   }

   // Specify target
   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strRemoteFolder.c_str());
   
   strBuf += strRemoteFileName;
   headerlist = curl_slist_append(headerlist, strBuf.c_str());

   curl_easy_setopt(m_pCurlSession, CURLOPT_POSTQUOTE, headerlist);
   curl_easy_setopt(m_pCurlSession, CURLOPT_NOBODY, 1L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADER, 1L);

   CURLcode res = Perform();

   // Check for errors
   if (res != CURLE_OK) {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_REMOVE_FORMAT, strRemoteFile.c_str(), res, curl_easy_strerror(res)));
   } else
      bRet = true;

   // clean up the FTP commands list
   curl_slist_free_all(headerlist);

   return bRet;
}

/**
 * @brief requests the mtime (epoch) and the size of a remote file.
 *
 * @param [in] strRemoteFile URN of the remote file encoded in UTF-8 format.
 * @param [out] oFileInfo time_t will be updated with the file's mtime and size.
 *
 * @retval true   Successfully gathered the file info time and updated
 * "oFileInfo"
 * @retval false  The infos couldn't be requested.
 *
 * Example Usage:
 * @code
 *    struct FileInfo& oFileInfo;
 *    bool bRes = oFTPClient.GetFileTime("pictures/muahahaha.jpg", tFileTime);
 *    if (bRes) { assert(oFileInfo.tFileMTime > 0); assert(oFileInfo.dFileSize >
 * 0); } std::cout << ctime(oFileInfo.tFileMTime) << std::endl; // Sat Aug 06
 * 15:04:45 2016
 * @endcode
 */
bool CFTPClient::Info(const std::string &strRemoteFile, struct FileInfo &oFileInfo) const {
   if (strRemoteFile.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   bool bRes = false;

   oFileInfo.tFileMTime = 0;
   oFileInfo.dFileSize  = 0.0;

   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, ParseURL(strRemoteFile).c_str());

   /* No download if the file */
   curl_easy_setopt(m_pCurlSession, CURLOPT_NOBODY, 1L);

   /* Ask for filetime */
   curl_easy_setopt(m_pCurlSession, CURLOPT_FILETIME, 1L);

   /* No header output: TODO 14.1 http-style HEAD output for ftp */
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADERFUNCTION, ThrowAwayCallback);
   curl_easy_setopt(m_pCurlSession, CURLOPT_HEADER, 0L);

   CURLcode res = Perform();

   if (CURLE_OK == res) {
      long lFileTime = -1;

      res = curl_easy_getinfo(m_pCurlSession, CURLINFO_FILETIME, &lFileTime);
      if (CURLE_OK == res && lFileTime >= 0) {
         oFileInfo.tFileMTime = static_cast<time_t>(lFileTime);
         bRes                 = true;
      }

      res = curl_easy_getinfo(m_pCurlSession, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &oFileInfo.dFileSize);
      if (CURLE_OK != res || oFileInfo.dFileSize < 0.0) {
         bRes = false;
      }
   } else if (m_eSettingsFlags & ENABLE_LOG)
      m_oLog(StringFormat(LOG_ERROR_CURL_FILETIME_FORMAT, strRemoteFile.c_str(), res, curl_easy_strerror(res)));

   return bRes;
}

/**
 * @brief lists a remote folder
 * the list can contain only names or can be detailed
 * entries/names will be separated with LF ('\n')
 *
 * @param [in] strRemoteFolder URL of the remote location to be listed encoded in UTF-8 format.
 * @param [out] strList will contain the directory entries (detailed or not) encoded in UTF-8 format.
 * @param [in] bOnlyNames detailed list or only names
 *
 * @retval true   Successfully listed the remote folder and updated FtpFiles.
 * @retval false  The remote folder couldn't be listed.
 *
 * Example Usage:
 * @code
 *    std::string strList;
 *    m_pFTPClient->GetFileList("/", strList, false);
 *    // lists the root of the remote FTP server with all the infos.
 * @endcode
 */
bool CFTPClient::List(const std::string &strRemoteFolder, std::string &strList, bool bOnlyNames /* = true */) const {
   if (strRemoteFolder.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   bool bRet = false;

   std::string strRemoteFile = ParseURL(strRemoteFolder);

   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strRemoteFile.c_str());

   if (bOnlyNames) curl_easy_setopt(m_pCurlSession, CURLOPT_DIRLISTONLY, 1L);

   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteInStringCallback);
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &strList);

   CURLcode res = Perform();

   if (CURLE_OK == res)
      bRet = true;
   else if (m_eSettingsFlags & ENABLE_LOG)
      m_oLog(StringFormat(LOG_ERROR_CURL_FILELIST_FORMAT, strRemoteFolder.c_str(), res, curl_easy_strerror(res)));

   return bRet;
}

/**
 * @brief downloads a remote file
 *
 * @param [in] strLocalFile complete path of the downloaded file encoded in UTF-8 format.
 * @param [in] strRemoteFile URL of the remote file encoded in UTF-8 format.
 *
 * @retval true   Successfully downloaded the file.
 * @retval false  The file couldn't be downloaded. Check the log messages for
 * more information.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->DownloadFile("C:\\Items_To_Upload\\imagination.jpg",
 * "upload/pictures/imagination.jpg");
 * @endcode
 */
bool CFTPClient::DownloadFile(const std::string &strLocalFile, const std::string &strRemoteFile) const {
   if (strLocalFile.empty() || strRemoteFile.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   bool bRet = false;

   std::string strFile = ParseURL(strRemoteFile);

   std::ofstream ofsOutput;
   ofsOutput.open(
       #ifdef LINUX
       strLocalFile, // UTF-8
       #else
       Utf8ToUtf16(strLocalFile),
       #endif
       std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

   if (ofsOutput) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strFile.c_str());
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteToFileCallback);
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &ofsOutput);

      CURLcode res = Perform();

      if (res != CURLE_OK) {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat(LOG_ERROR_CURL_GETFILE_FORMAT, m_strServer.c_str(), strRemoteFile.c_str(), res, curl_easy_strerror(res)));
      } else
         bRet = true;

      ofsOutput.close();

      if (!bRet) remove(strLocalFile.c_str());
   } else if (m_eSettingsFlags & ENABLE_LOG)
      m_oLog(LOG_ERROR_FILE_GETFILE_FORMAT);

   return bRet;
}
/**
 * @brief downloads a remote file to memory
 *
 * @param [in] strRemoteFile URI of remote file encoded in UTF-8 format.
 * @param [out] data vector of bytes
 *
 * @retval true   Successfully downloaded the file.
 * @retval false  The file couldn't be downloaded. Check the log messages for
 * more information.
 */
bool CFTPClient::DownloadFile(const std::string &strRemoteFile, std::vector<char> &data) const {
   if (strRemoteFile.empty()) return false;
   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);
      return false;
   }
   curl_easy_reset(m_pCurlSession);
   std::string strFile = ParseURL(strRemoteFile);

   data.clear();

   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strFile.c_str());
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteToMemory);
   curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &data);

   CURLcode res = Perform();

   if (res != CURLE_OK) {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_GETFILE_FORMAT, m_strServer.c_str(), strRemoteFile.c_str(), res, curl_easy_strerror(res)));
      return false;
   } else
      return true;
}

/**
 * @brief downloads all elements according that match the wildcarded URL
 *
 * @param [in] strLocalFile Complete path where the elements will be downloaded encoded in UTF-8 format.
 * @param [in] strRemoteWildcard Wildcarded pattern to be downloaded encoded in UTF-8 format.
 *
 * @retval true   All the elements have been downloaded.
 * @retval false  Some or all files or dir have not been downloaded or resp.
 * created.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->DownloadWildcard("", "");
 * @endcode
 */
bool CFTPClient::DownloadWildcard(const std::string &strLocalDir, const std::string &strRemoteWildcard) const {
   if (strLocalDir.empty() || strRemoteWildcard.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   bool bRet = false;

   WildcardTransfersCallbackData data;
#ifdef LINUX
   data.strOutputPath = strLocalDir + ((strLocalDir.back() != '/') ? "/" : "");
#else
   data.strOutputPath = strLocalDir + ((strLocalDir.back() != '\\') ? "\\" : "");
#endif

   std::string strPattern = ParseURL(strRemoteWildcard);

   struct stat info;
   if (stat(data.strOutputPath.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))  // S_ISDIR() doesn't exist on windows
   {
      /* 0. turn on wildcard matching */
      curl_easy_setopt(m_pCurlSession, CURLOPT_WILDCARDMATCH, 1L);

      /* 1. callback is called before download of concrete file started */
      curl_easy_setopt(m_pCurlSession, CURLOPT_CHUNK_BGN_FUNCTION, FileIsComingCallback);

      /* 2. this callback will write contents into files */
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEFUNCTION, WriteItCallback);

      /* 3. callback is called after data from the file have been transferred */
      curl_easy_setopt(m_pCurlSession, CURLOPT_CHUNK_END_FUNCTION, FileIsDownloadedCallback);

      /* put transfer data into callbacks */
      curl_easy_setopt(m_pCurlSession, CURLOPT_CHUNK_DATA, &data);
      curl_easy_setopt(m_pCurlSession, CURLOPT_WRITEDATA, &data);
      /* curl_easy_setopt(m_pCurlSession, CURLOPT_VERBOSE, 1L); */

      /* set an URL containing wildcard pattern (only in the last part) */
      curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strPattern.c_str());

      /* and start transfer! */
      CURLcode res = Perform();

      /* in case we have an empty FTP folder, error 78 will be returned */
      if (res != CURLE_OK && res != CURLE_REMOTE_FILE_NOT_FOUND) {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(
                StringFormat(LOG_ERROR_CURL_GETWILD_FORMAT, m_strServer.c_str(), strRemoteWildcard.c_str(), res, curl_easy_strerror(res)));
      }
      /* folders need to be copied integrally */
      else if (!data.vecDirList.empty() && strRemoteWildcard.back() == '*') {
         std::string strBaseUrl = strRemoteWildcard.substr(0, strRemoteWildcard.length() - 1);

         if (!strBaseUrl.empty() && strBaseUrl.back() != '/') strBaseUrl += "/";

         // recursively download directories
         bRet = true;
         for (const auto &Dir : data.vecDirList) {
            if ((Dir == ".") || (Dir == "..")) continue;
            if (!DownloadWildcard(data.strOutputPath + Dir, strBaseUrl + Dir + "/*")) {
               m_oLog(
                   StringFormat(LOG_ERROR_CURL_GETWILD_REC_FORMAT, (strBaseUrl + Dir + "/*").c_str(), (data.strOutputPath + Dir).c_str()));
               bRet = false;
            }
         }
      } else
         bRet = true;
   } else if (m_eSettingsFlags & ENABLE_LOG)
      m_oLog(StringFormat(LOG_ERROR_DIR_GETWILD_FORMAT, data.strOutputPath.c_str()));

   return bRet;
}

/**
 * @brief uploads a user data using readFn to a remote folder.
 *
 * @param [in] readFn Reading function, corresponds to <a href="https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html">CURLOPT_READFUNCTION</a>.
 * @param [in] userData user data passed to readFn as last parameter.
 * @param [in] strRemoteFile Complete URN of the remote location (with the file
 * name) encoded in UTF-8 format.
 * @param [in] bCreateDir Enable or disable creation of remote missing
 * directories contained in the URN.
 *
 * @retval true   Data successfully uploaded.
 * @retval false  Data couldn't be uploaded. Check the log messages for more
 * information.
 */
bool CFTPClient::UploadFile(CFTPClient::CurlReadFn readFn, void *userData, const std::string &strRemoteFile,
                            const bool &bCreateDir, curl_off_t fileSize) const {
   if (readFn == nullptr || strRemoteFile.empty())
      return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   std::string strLocalRemoteFile = ParseURL(strRemoteFile);

   bool bRes = false;

   /* specify target */
   curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strLocalRemoteFile.c_str());

   /* we want to use our own read function */
   curl_easy_setopt(m_pCurlSession, CURLOPT_READFUNCTION, readFn);

   /* now specify which file to upload */
   curl_easy_setopt(m_pCurlSession, CURLOPT_READDATA, userData);

   /* Set the size of the file to upload (optional).  If you give a *_LARGE
   option you MUST make sure that the type of the passed-in argument is a
   curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
   make sure that to pass in a type 'long' argument. */
   curl_easy_setopt(m_pCurlSession, CURLOPT_INFILESIZE_LARGE, fileSize);

   /* enable uploading */
   curl_easy_setopt(m_pCurlSession, CURLOPT_UPLOAD, 1L);

   if (bCreateDir) curl_easy_setopt(m_pCurlSession, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR);

   CURLcode res = Perform();

   if (res != CURLE_OK) {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat(LOG_ERROR_CURL_UPLOAD_FORMAT, strRemoteFile.c_str(), res, curl_easy_strerror(res)));
   } else
      bRes = true;

   return bRes;
}

/**
 * @brief uploads data from a stream to a remote folder.
 *
 * @param [in] inputStream Stream containing data which will be uploaded.
 * @param [in] strRemoteFile Complete URN of the remote location (with the file
 * name) encoded in UTF-8 format.
 * @param [in] bCreateDir Enable or disable creation of remote missing
 * directories contained in the URN.
 * @param [in] fileSize
 *
 * @retval true   Successfully uploaded the inputStream.
 * @retval false  The inputStream couldn't be uploaded. Check the log messages for more
 * information.
 */
bool CFTPClient::UploadFile(std::istream &inputStream, const std::string &strRemoteFile, const bool &bCreateDir,
                            curl_off_t fileSize) const {
   if ( !inputStream )
      return false;

   return UploadFile(ReadFromStreamCallback, static_cast<void *>(&inputStream), strRemoteFile, bCreateDir, fileSize);
}

/**
 * @brief uploads a local file to a remote folder.
 *
 * @param [in] strLocalFile Complete path of the file to upload encoded in UTF-8 format.
 * @param [in] strRemoteFile Complete URN of the remote location (with the file
 * name) encoded in UTF-8 format.
 * @param [in] bCreateDir Enable or disable creation of remote missing
 * directories contained in the URN.
 *
 * @retval true   Successfully uploaded the file.
 * @retval false  The file couldn't be uploaded. Check the log messages for more
 * information.
 *
 * Example Usage:
 * @code
 *    m_pFTPClient->UploadFile("C:\\Items_To_Upload\\imagination.jpg",
 * "upload/pictures/imagination.jpg", true);
 *    // C:\\Items_To_Upload\\imagination.jpg will be uploaded to
 * ftp://127.0.0.1/upload/pictures/imagination.jpg
 *    // "upload" and "pictures" remote directories will be created
 *    // if they don't exist and if the connected user has the proper rights.
 * @endcode
 */
bool CFTPClient::UploadFile(const std::string &strLocalFile, const std::string &strRemoteFile, const bool &bCreateDir) const {
   if (strLocalFile.empty() || strRemoteFile.empty()) return false;

   std::ifstream InputFile;

   struct stat file_info;
   bool bRes = false;

   /* get the file size of the local file */
   #ifdef LINUX
   if (stat(strLocalFile.c_str(), &file_info) == 0) {
      InputFile.open(strLocalFile, std::ifstream::in | std::ifstream::binary);
   #else
   static_assert(sizeof(struct stat) == sizeof(struct _stat64i32), "Oh oh !");
   std::wstring wstrLocalFile = Utf8ToUtf16(strLocalFile);
   if (_wstat64i32(wstrLocalFile.c_str(), reinterpret_cast<struct _stat64i32*>(&file_info)) == 0) {
      InputFile.open(wstrLocalFile, std::ifstream::in | std::ifstream::binary);
   #endif
      if (!InputFile) {
         if (m_eSettingsFlags & ENABLE_LOG) m_oLog(StringFormat(LOG_ERROR_FILE_UPLOAD_FORMAT, strLocalFile.c_str()));

         return false;
      }

      bRes = UploadFile(InputFile, strRemoteFile, bCreateDir, file_info.st_size);
   }
   InputFile.close();

   return bRes;
}

bool CFTPClient::AppendFile(const std::string &strLocalFile, const size_t fileOffset, const std::string &strRemoteFile,
                            const bool &bCreateDir) const {
   if (strLocalFile.empty() || strRemoteFile.empty()) return false;

   if (!m_pCurlSession) {
      if (m_eSettingsFlags & ENABLE_LOG) m_oLog(LOG_ERROR_CURL_NOT_INIT_MSG);

      return false;
   }
   // Reset is mandatory to avoid bad surprises
   curl_easy_reset(m_pCurlSession);

   std::ifstream InputFile;
   std::string strLocalRemoteFile = ParseURL(strRemoteFile);

   struct stat file_info;
   bool bRes = false;

/* get the file size of the local file */
#ifdef LINUX
   if (stat(strLocalFile.c_str(), &file_info) == 0) {
      InputFile.open(strLocalFile, std::ifstream::in | std::ifstream::binary);
#else
   static_assert(sizeof(struct stat) == sizeof(struct _stat64i32), "Oh oh !");
   std::wstring wstrLocalFile = Utf8ToUtf16(strLocalFile);
   if (_wstat64i32(wstrLocalFile.c_str(), reinterpret_cast<struct _stat64i32 *>(&file_info)) == 0) {
      InputFile.open(wstrLocalFile, std::ifstream::in | std::ifstream::binary);
#endif
      if (!InputFile) {
         if (m_eSettingsFlags & ENABLE_LOG) m_oLog(StringFormat(LOG_ERROR_FILE_UPLOAD_FORMAT, strLocalFile.c_str()));

         return false;
      }

      // check of the offset is less than the file size
      if (fileOffset >= file_info.st_size) {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog("ERROR Incorrect offset !");  // if this code is OK use existing coding style for log msgs
         return false;
      }

      InputFile.seekg(fileOffset, InputFile.beg);  // Sets the position of the next character to be extracted from the input stream.

      /* specify target */
      curl_easy_setopt(m_pCurlSession, CURLOPT_URL, strLocalRemoteFile.c_str());

      /* we want to use our own read function */
      curl_easy_setopt(m_pCurlSession, CURLOPT_READFUNCTION, ReadFromStreamCallback);

      /* now specify which file to upload */
      curl_easy_setopt(m_pCurlSession, CURLOPT_READDATA, &InputFile);

      /* Set the size of the file to upload (optional).  If you give a *_LARGE
      option you MUST make sure that the type of the passed-in argument is a
      curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
      make sure that to pass in a type 'long' argument. */
      curl_easy_setopt(m_pCurlSession, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(file_info.st_size - fileOffset));  // Important !

      /* enable uploading */
      curl_easy_setopt(m_pCurlSession, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_APPEND, 1L);

      if (bCreateDir) curl_easy_setopt(m_pCurlSession, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR);

      // TODO add the possibility to rename the file upon upload finish....

      CURLcode res = Perform();

      if (res != CURLE_OK) {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat(LOG_ERROR_CURL_UPLOAD_FORMAT, strLocalFile.c_str(), res, curl_easy_strerror(res)));
      } else
         bRes = true;
   }
   InputFile.close();

   return bRes;
}

/**
 * @brief performs the chosen FTP request
 * sets up the common settings (Timeout, proxy,...)
 *
 *
 * @retval true   Successfully performed the request.
 * @retval false  The request couldn't be performed.
 *
 */
 CURLcode CFTPClient::Perform() const {
   CURLcode res = CURLE_OK;

   curl_easy_setopt(m_pCurlSession, CURLOPT_PORT, m_uPort);
   curl_easy_setopt(m_pCurlSession, CURLOPT_USERPWD, (m_strUserName + ":" + m_strPassword).c_str());

   if (m_bActive) curl_easy_setopt(m_pCurlSession, CURLOPT_FTPPORT, "-");

   if (m_iCurlTimeout > 0) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_TIMEOUT, m_iCurlTimeout);
   }
   if (m_bNoSignal) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOSIGNAL, 1L);
   }

   if (!m_strProxy.empty()) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROXY, m_strProxy.c_str());
      curl_easy_setopt(m_pCurlSession, CURLOPT_HTTPPROXYTUNNEL, 1L);
       
       if (!m_strProxyUserPwd.empty()) {
           curl_easy_setopt(m_pCurlSession, CURLOPT_PROXYUSERPWD, m_strProxyUserPwd.c_str());
       }

      // use only plain PASV
      if (!m_bActive) {
         curl_easy_setopt(m_pCurlSession, CURLOPT_FTP_USE_EPSV, 1L);
      }
   }

   if (m_bProgressCallbackSet) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROGRESSFUNCTION, *GetProgressFnCallback());
      curl_easy_setopt(m_pCurlSession, CURLOPT_PROGRESSDATA, &m_ProgressStruct);
      curl_easy_setopt(m_pCurlSession, CURLOPT_NOPROGRESS, 0L);
   }

   if (m_eFtpProtocol == FTP_PROTOCOL::FTPS || m_eFtpProtocol == FTP_PROTOCOL::FTPES)
      /* We activate SSL and we require it for both control and data */
      curl_easy_setopt(m_pCurlSession, CURLOPT_USE_SSL, CURLUSESSL_ALL);

   if (m_eFtpProtocol == FTP_PROTOCOL::SFTP && m_eSettingsFlags & ENABLE_SSH_AGENT)
      /* We activate ssh agent. For this to work you need
      to have ssh-agent running (type set | grep SSH_AGENT to check) or
      pageant on Windows (there is an icon in systray if so) */
      curl_easy_setopt(m_pCurlSession, CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_AGENT);

   // SSL
   if (!m_strSSLCertFile.empty()) curl_easy_setopt(m_pCurlSession, CURLOPT_SSLCERT, m_strSSLCertFile.c_str());

   if (!m_strSSLKeyFile.empty()) curl_easy_setopt(m_pCurlSession, CURLOPT_SSLKEY, m_strSSLKeyFile.c_str());

   if (!m_strSSLKeyPwd.empty()) curl_easy_setopt(m_pCurlSession, CURLOPT_KEYPASSWD, m_strSSLKeyPwd.c_str());

   curl_easy_setopt(m_pCurlSession, CURLOPT_SSL_VERIFYHOST, (m_bInsecure) ? 0L : 2L);
   curl_easy_setopt(m_pCurlSession, CURLOPT_SSL_VERIFYPEER, (m_bInsecure) ? 0L : 1L);

#ifdef DEBUG_CURL
   StartCurlDebug();
#endif

   // Perform the requested operation
   res = curl_easy_perform(m_pCurlSession);

#ifdef DEBUG_CURL
   EndCurlDebug();
#endif

   return res;
}

// STRING HELPERS

/**
 * @brief returns a formatted string
 *
 * @param [in] strFormat string with one or many format specifiers
 * @param [in] parameters to be placed in the format specifiers of strFormat
 *
 * @retval string formatted string
 */
std::string CFTPClient::StringFormat(std::string strFormat, ...) {
   va_list args;
   va_start(args, strFormat);
   size_t len = std::vsnprintf(NULL, 0, strFormat.c_str(), args);
   va_end(args);
   std::vector<char> vec(len + 1);
   va_start(args, strFormat);
   std::vsnprintf(&vec[0], len + 1, strFormat.c_str(), args);
   va_end(args);
   return &vec[0];
}

void CFTPClient::ReplaceString(std::string &strSubject, const std::string &strSearch, const std::string &strReplace) {
   if (strSearch.empty()) return;

   size_t pos = 0;
   while ((pos = strSubject.find(strSearch, pos)) != std::string::npos) {
      strSubject.replace(pos, strSearch.length(), strReplace);
      pos += strReplace.length();
   }
}
/*
void CFTPClient::ReplaceString(std::string& str, const std::string& from, const
std::string& to)
{
   if(from.empty())
      return;

   string wsRet;
   wsRet.reserve(str.length());
   size_t start_pos = 0, pos;

   while((pos = str.find(from, start_pos)) != std::string::npos)
   {
      wsRet += str.substr(start_pos, pos - start_pos);
      wsRet += to;
      pos += from.length();
      start_pos = pos;
   }

   wsRet += str.substr(start_pos);
   str.swap(wsRet); // faster than str = wsRet;
}
*/

// CURL CALLBACKS

size_t CFTPClient::ThrowAwayCallback(void *ptr, size_t size, size_t nmemb, void *data) {
    /* we are not interested in the headers itself,
    so we only return the size we would have saved ... */

    UNUSED(ptr)
    UNUSED(data)
    return size * nmemb;
}

/**
 * @brief stores the server response in a string
 *
 * @param ptr pointer of max size (size*nmemb) to read data from it
 * @param size size parameter
 * @param nmemb memblock parameter
 * @param data pointer to user data (string)
 *
 * @return (size * nmemb)
 */
size_t CFTPClient::WriteInStringCallback(void *ptr, size_t size, size_t nmemb, void *data) {
   std::string *strWriteHere = reinterpret_cast<std::string *>(data);
   if (strWriteHere != nullptr) {
      strWriteHere->append(reinterpret_cast<char *>(ptr), size * nmemb);
      return size * nmemb;
   }
   return 0;
}

/**
 * @brief stores the server response in an already opened file stream
 * used by DownloadFile()
 *
 * @param buff pointer of max size (size*nmemb) to read data from it
 * @param size size parameter
 * @param nmemb memblock parameter
 * @param userdata pointer to user data (file stream)
 *
 * @return (size * nmemb)
 */
size_t CFTPClient::WriteToFileCallback(void *buff, size_t size, size_t nmemb, void *data) {
   if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1) || (data == nullptr)) return 0;

   std::ofstream *pFileStream = reinterpret_cast<std::ofstream *>(data);
   if (pFileStream->is_open()) {
      pFileStream->write(reinterpret_cast<char *>(buff), size * nmemb);
   }

   return size * nmemb;
}
/**
 * @brief stores the server response in std::vector<char>
 *
 * @param buff pointer of max size (size*nmemb) to read data from it
 * @param size size parameter
 * @param nmemb memblock parameter
 * @param userdata pointer to user data (file stream)
 *
 * @return (size * nmemb)
 */
size_t CFTPClient::WriteToMemory(void *buff, size_t size, size_t nmemb, void *data) {
   if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1) || (data == nullptr)) return 0;

   auto *vec    = reinterpret_cast<std::vector<char> *>(data);
   size_t ssize = size * nmemb;
   std::copy((char *)buff, (char *)buff + ssize, std::back_inserter(*vec));

   return ssize;
}
/**
 * @brief reads the content of an already opened file stream
 * used by UploadFile()
 *
 * @param ptr pointer of max size (size*nmemb) to write data to it
 * @param size size parameter
 * @param nmemb memblock parameter
 * @param stream pointer to user data (input stream)
 *
 * @return (size * nmemb)
 */
size_t CFTPClient::ReadFromStreamCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
   auto *pFileStream = reinterpret_cast<std::istream *>(stream);
   if (!pFileStream->fail()) {
      pFileStream->read(reinterpret_cast<char *>(ptr), size * nmemb);
      return pFileStream->gcount();
   }
   return 0;
}

// WILDCARD DOWNLOAD CALLBACKS

/**
 * @brief will be called before processing an incoming item (file or dir)
 *
 * @param finfo
 * @param data
 * @param remains
 *
 * @return CURL_CHUNK_BGN_FUNC_FAIL (abort perform) or CURL_CHUNK_BGN_FUNC_OK
 * (continue)
 */
long CFTPClient::FileIsComingCallback(struct curl_fileinfo *finfo, WildcardTransfersCallbackData *data, int remains) {
   // printf("%3d %40s %10luB ", remains, finfo->filename, (unsigned
   // long)finfo->size);
    UNUSED(remains)

    switch (finfo->filetype) {
    case CURLFILETYPE_DIRECTORY:
        // printf(" DIR\n");
        data->vecDirList.push_back(finfo->filename);
#ifdef LINUX
        if (mkdir((data->strOutputPath + finfo->filename).c_str(), ACCESSPERMS) != 0 && errno != EEXIST)
#else
        if (_mkdir((data->strOutputPath + finfo->filename).c_str()) != 0 && errno != EEXIST)
#endif
        {
            std::cerr << "Problem creating directory (errno=" << errno << "): " << data->strOutputPath + finfo->filename << std::endl;
            return CURL_CHUNK_BGN_FUNC_FAIL;
        }
        /*else
         {
            printf("Directory '%s' was successfully created\n", finfo->filename);
            #ifdef LINUX
            system("ls \\testtmp");
            #else
            system("dir \\testtmp");
            #endif
            #ifdef LINUX
            if (rmdir((data->strOutputPath + finfo->filename).c_str()) == 0)
            #else
            if (_rmdir((data->strOutputPath + finfo->filename).c_str()) == 0)
            #endif
               printf("Directory '%s' was successfully removed\n",
         (data->strOutputPath + finfo->filename).c_str()); else printf("Problem
         removing directory '%s'\n", (data->strOutputPath +
         finfo->filename).c_str());
         }*/
        break;
    case CURLFILETYPE_FILE:
        // printf("FILE ");
        /* do not transfer files >= 50B */
        // if (finfo->size > 50)
        //{
        // printf("SKIPPED\n");
        // return CURL_CHUNK_BGN_FUNC_SKIP;
        //}
        data->ofsOutput.open(
            #ifdef LINUX
                    data->strOutputPath + finfo->filename,
            #else
                    Utf8ToUtf16(data->strOutputPath + finfo->filename),
            #endif
                    std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
        if (!data->ofsOutput.is_open()) {
            return CURL_CHUNK_BGN_FUNC_FAIL;
        }
        break;
    default:
        // printf("OTHER\n");
        break;
    }

   return CURL_CHUNK_BGN_FUNC_OK;
}

/**
 * @brief will be called before after downloading a file
 *
 * @param data
 *
 * @return CURL_CHUNK_END_FUNC_OK (continue performing the request)
 */
long CFTPClient::FileIsDownloadedCallback(WildcardTransfersCallbackData *data) {
   if (data->ofsOutput.is_open()) {
      // printf("DOWNLOADED\n");
      data->ofsOutput.close();
   }
   return CURL_CHUNK_END_FUNC_OK;
}

/**
 * @brief will be called to write a chunk of the file before after downloading a
 * file
 *
 * @param buff pointer of max size (size*nmemb) to read data from it
 * @param size size parameter
 * @param nmemb memblock parameter
 * @param cb_data pointer to user data (string)
 *
 * @return number of written bytes
 */
size_t CFTPClient::WriteItCallback(char *buff, size_t size, size_t nmemb, void *cb_data) {
   WildcardTransfersCallbackData *data = reinterpret_cast<WildcardTransfersCallbackData *>(cb_data);
   size_t written                      = 0;

   if (data->ofsOutput.is_open()) {
      data->ofsOutput.write(buff, size * nmemb);
      written = nmemb;
   }
   /*else
   {
      std::cout.write(buff, size * nmemb);
      written = nmemb;
   }*/
   return written;
}

// CURL DEBUG INFO CALLBACKS

#ifdef DEBUG_CURL
void CFTPClient::SetCurlTraceLogDirectory(const std::string &strPath) {
   s_strCurlTraceLogDirectory = strPath;

   if (!s_strCurlTraceLogDirectory.empty()
#ifdef WINDOWS
       && s_strCurlTraceLogDirectory.back() != '\\') {
      s_strCurlTraceLogDirectory += '\\';
   }
#else
       && s_strCurlTraceLogDirectory.back() != '/') {
      s_strCurlTraceLogDirectory += '/';
   }
#endif
}

int CFTPClient::DebugCallback(CURL *curl, curl_infotype curl_info_type, char *pszTrace, size_t usSize, void *pFile) {
   std::string strText;
   std::string strTrace(pszTrace, usSize);

   switch (curl_info_type) {
      case CURLINFO_TEXT:
         strText = "# Information : ";
         break;
      case CURLINFO_HEADER_OUT:
         strText = "-> Sending header : ";
         break;
      case CURLINFO_DATA_OUT:
         strText = "-> Sending data : ";
         break;
      case CURLINFO_SSL_DATA_OUT:
         strText = "-> Sending SSL data : ";
         break;
      case CURLINFO_HEADER_IN:
         strText = "<- Receiving header : ";
         break;
      case CURLINFO_DATA_IN:
         strText = "<- Receiving unencrypted data : ";
         break;
      case CURLINFO_SSL_DATA_IN:
         strText = "<- Receiving SSL data : ";
         break;
      default:
         break;
   }

   std::ofstream *pofTraceFile = reinterpret_cast<std::ofstream *>(pFile);
   if (pofTraceFile == nullptr) {
      std::cout << "[DEBUG] cURL debug log [" << curl_info_type << "]: "
                << " - " << strTrace;
   } else {
      (*pofTraceFile) << strText << strTrace;
   }

   return 0;
}

void CFTPClient::StartCurlDebug() const {
   if (!m_ofFileCurlTrace.is_open()) {
      curl_easy_setopt(m_pCurlSession, CURLOPT_VERBOSE, 1L);
      curl_easy_setopt(m_pCurlSession, CURLOPT_DEBUGFUNCTION, DebugCallback);

      std::string strFileCurlTraceFullName(s_strCurlTraceLogDirectory);
      if (!strFileCurlTraceFullName.empty()) {
         char szDate[32];
         memset(szDate, 0, 32);
         time_t tNow;
         time(&tNow);
         // new trace file for each hour
         strftime(szDate, 32, "%Y%m%d_%H", localtime(&tNow));
         strFileCurlTraceFullName += "TraceLog_";
         strFileCurlTraceFullName += szDate;
         strFileCurlTraceFullName += ".txt";

         m_ofFileCurlTrace.open(strFileCurlTraceFullName, std::ifstream::app | std::ifstream::binary);

         if (m_ofFileCurlTrace) curl_easy_setopt(m_pCurlSession, CURLOPT_DEBUGDATA, &m_ofFileCurlTrace);
      }
   }
}

void CFTPClient::EndCurlDebug() const {
   if (m_ofFileCurlTrace && m_ofFileCurlTrace.is_open()) {
      m_ofFileCurlTrace << "###########################################" << std::endl;
      m_ofFileCurlTrace.close();
   }
}
#endif

#ifdef WINDOWS
std::string CFTPClient::AnsiToUtf8(const std::string &codepage_str) {
   // Transcode Windows ANSI to UTF-16
   int size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, codepage_str.c_str(), codepage_str.length(), nullptr, 0);
   std::wstring utf16_str(size, '\0');
   MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, codepage_str.c_str(), codepage_str.length(), &utf16_str[0], size);

   // Transcode UTF-16 to UTF-8
   int utf8_size = WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), utf16_str.length(), nullptr, 0, nullptr, nullptr);
   std::string utf8_str(utf8_size, '\0');
   WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), utf16_str.length(), &utf8_str[0], utf8_size, nullptr, nullptr);

   return utf8_str;
}

std::wstring CFTPClient::Utf8ToUtf16(const std::string &str) {
   std::wstring ret;
   int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
   if (len > 0) {
      ret.resize(len);
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len);
   }
   return ret;
}
#endif

}  // namespace embeddedmz
