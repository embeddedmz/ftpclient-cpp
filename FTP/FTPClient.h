/*
 * @file FTPClient.h
 * @brief libcurl wrapper for FTP requests
 *
 * @author Mohamed Amine Mzoughi <mohamed-amine.mzoughi@laposte.net>
 * @date 2017-01-17
 */

#ifndef INCLUDE_FTPCLIENT_H_
#define INCLUDE_FTPCLIENT_H_

#define FTPCLIENT_VERSION "FTPCLIENT_VERSION_1.0.0"

#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <cstddef>  // std::size_t
#include <cstdio>   // snprintf
#include <cstdlib>
#include <cstring>  // strerror, strlen, memcpy, strcpy
#include <ctime>
#ifndef LINUX
#include <direct.h>  // mkdir
#endif
#include <stdarg.h>  // va_start, etc.
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>  // std::unique_ptr
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "CurlHandle.h"

namespace embeddedmz {

class CFTPClient {
  public:
   // Public definitions
   using ProgressFnCallback = std::function<int(void *, double, double, double, double)>;
   using LogFnCallback      = std::function<void(const std::string &)>;

   // Used to download many items at once
   struct WildcardTransfersCallbackData {
      std::ofstream ofsOutput;
      std::string strOutputPath;
      std::vector<std::string> vecDirList;
      // will be used to call GetWildcard recursively to download subdirectories
      // content...
   };

   // Progress Function Data Object - parameter void* of ProgressFnCallback
   // references it
   struct ProgressFnStruct {
      ProgressFnStruct() : dLastRunTime(0), pCurl(nullptr), pOwner(nullptr) {}
      double dLastRunTime;
      CURL *pCurl;
      /* owner of the CFTPClient object. can be used in the body of the progress
       * function to send signals to the owner (e.g. to update a GUI's progress
       * bar)
       */
      void *pOwner;
   };

   // See Info method.
   struct FileInfo {
      time_t tFileMTime;
      double dFileSize;
   };

   enum SettingsFlag {
      NO_FLAGS   = 0x00,
      ENABLE_LOG = 0x01,
      ENABLE_SSH = 0x02,  // only for SFTP
      ALL_FLAGS  = 0xFF
   };

   enum class FTP_PROTOCOL : unsigned char {
      // These three protocols below should not be confused with the SFTP
      // protocol. SFTP is an entirely different file transfer protocol
      // that runs over SSH2.
      FTP,  // Plain, unencrypted FTP that defaults over port 21. Most web browsers
            // support basic FTP.

      FTPS, /* Implicit SSL/TLS encrypted FTP that works just like HTTPS.
             * Security is enabled with SSL as soon as the connection starts.
             * The default FTPS port is 990. This protocol was the first version
             * of encrypted FTP available, and while considered deprecated, is
             * still widely used. None of the major web browsers support FTPS. */

      FTPES, /* Explicit FTP over SSL/TLS. This starts out as plain FTP over port
              * 21, but through special FTP commands is upgraded to TLS/SSL
              * encryption. This upgrade usually occurs before the user
              * credentials are sent over the connection. FTPES is a somewhat
              * newer form of encrypted FTP (although still over a decade old),
              * and is considered the preferred way to establish encrypted
              * connections because it can be more firewall friendly. None of the
              * major web browsers support FTPES. */

      SFTP
   };

   /* Please provide your logger thread-safe routine, otherwise, you can turn off
    * error log messages printing by not using the flag ALL_FLAGS or ENABLE_LOG
    */
   explicit CFTPClient(LogFnCallback oLogger = [](const std::string &) {});
   virtual ~CFTPClient();

   // copy constructor and assignment operator are disabled
   CFTPClient(const CFTPClient &) = delete;
   CFTPClient &operator=(const CFTPClient &) = delete;

   // allow constructor and assignment operator are disabled
   CFTPClient(CFTPClient &&) = delete;
   CFTPClient &operator=(CFTPClient &&) = delete;

   // Setters - Getters (for unit tests)
   void SetProgressFnCallback(void *pOwner, const ProgressFnCallback &fnCallback);
   void SetProxy(const std::string &strProxy);
   inline void SetTimeout(const int &iTimeout) { m_iCurlTimeout = iTimeout; }
   inline void SetActive(const bool &bEnable) { m_bActive = bEnable; }
   inline void SetNoSignal(const bool &bNoSignal) { m_bNoSignal = bNoSignal; }
   inline auto GetProgressFnCallback() const { return m_fnProgressCallback.target<int (*)(void *, double, double, double, double)>(); }
   inline void *GetProgressFnCallbackOwner() const { return m_ProgressStruct.pOwner; }
   inline const std::string &GetProxy() const { return m_strProxy; }
   inline const int GetTimeout() const { return m_iCurlTimeout; }
   inline const unsigned GetPort() const { return m_uPort; }
   inline const bool GetActive() { return m_bActive; }
   inline const bool GetNoSignal() const { return m_bNoSignal; }
   inline const std::string &GetURL() const { return m_strServer; }
   inline const std::string &GetUsername() const { return m_strUserName; }
   inline const std::string &GetPassword() const { return m_strPassword; }
   inline const unsigned char GetSettingsFlags() const { return m_eSettingsFlags; }
   inline const FTP_PROTOCOL GetProtocol() const { return m_eFtpProtocol; }

   // Session
   const bool InitSession(const std::string &strHost, const unsigned &uPort, const std::string &strLogin, const std::string &strPassword,
                          const FTP_PROTOCOL &eFtpProtocol = FTP_PROTOCOL::FTP, const SettingsFlag &SettingsFlags = ALL_FLAGS);
   virtual const bool CleanupSession();
   const CURL *GetCurlPointer() const { return m_pCurlSession; }

   // FTP requests
   const bool CreateDir(const std::string &strNewDir) const;

   const bool RemoveDir(const std::string &strDir) const;

   const bool RemoveFile(const std::string &strRemoteFile) const;

   /* Checks a single file's size and mtime from an FTP server */
   const bool Info(const std::string &strRemoteFile, struct FileInfo &oFileInfo) const;

   const bool List(const std::string &strRemoteFolder, std::string &strList, bool bOnlyNames = true) const;

   const bool DownloadFile(const std::string &strLocalFile, const std::string &strRemoteFile) const;

   const bool DownloadFile(const std::string &strRemoteFile, std::vector<char> &data) const;

   const bool DownloadWildcard(const std::string &strLocalDir, const std::string &strRemoteWildcard) const;

   const bool UploadFile(const std::string &strLocalFile, const std::string &strRemoteFile, const bool &bCreateDir = false) const;

   // SSL certs
   void SetSSLCertFile(const std::string &strPath) { m_strSSLCertFile = strPath; }
   const std::string &GetSSLCertFile() const { return m_strSSLCertFile; }

   void SetSSLKeyFile(const std::string &strPath) { m_strSSLKeyFile = strPath; }
   const std::string &GetSSLKeyFile() const { return m_strSSLKeyFile; }

   void SetSSLKeyPassword(const std::string &strPwd) { m_strSSLKeyPwd = strPwd; }
   const std::string &GetSSLKeyPwd() const { return m_strSSLKeyPwd; }

#ifdef DEBUG_CURL
   static void SetCurlTraceLogDirectory(const std::string &strPath);
#endif

  private:
   /* common operations are performed here */
   inline const CURLcode Perform() const;
   inline std::string ParseURL(const std::string &strURL) const;

   // Curl callbacks
   static size_t WriteInStringCallback(void *ptr, size_t size, size_t nmemb, void *data);
   static size_t WriteToFileCallback(void *ptr, size_t size, size_t nmemb, void *data);
   static size_t ReadFromFileCallback(void *ptr, size_t size, size_t nmemb, void *stream);
   static size_t ThrowAwayCallback(void *ptr, size_t size, size_t nmemb, void *data);
   static size_t WriteToMemory(void *ptr, size_t size, size_t nmemb, void *data);

   // Wildcard transfers callbacks
   static long FileIsComingCallback(struct curl_fileinfo *finfo, WildcardTransfersCallbackData *data, int remains);
   static long FileIsDownloadedCallback(WildcardTransfersCallbackData *data);
   static size_t WriteItCallback(char *buff, size_t size, size_t nmemb, void *cb_data);

   // String Helpers
   static std::string StringFormat(std::string strFormat, ...);
   static void ReplaceString(std::string &strSubject, const std::string &strSearch, const std::string &strReplace);

// Curl Debug informations
#ifdef DEBUG_CURL
   static int DebugCallback(CURL *curl, curl_infotype curl_info_type, char *strace, size_t nSize, void *pFile);
   inline void StartCurlDebug() const;
   inline void EndCurlDebug() const;
#endif

   std::string m_strUserName;
   std::string m_strPassword;
   std::string m_strServer;
   std::string m_strProxy;

   bool m_bActive;  // For active FTP connections
   bool m_bNoSignal;
   unsigned m_uPort;

   FTP_PROTOCOL m_eFtpProtocol;
   SettingsFlag m_eSettingsFlags;

   // SSL
   std::string m_strSSLCertFile;
   std::string m_strSSLKeyFile;
   std::string m_strSSLKeyPwd;

   mutable CURL *m_pCurlSession;
   int m_iCurlTimeout;

   // Progress function
   ProgressFnCallback m_fnProgressCallback;
   ProgressFnStruct m_ProgressStruct;
   bool m_bProgressCallbackSet;

   // Log printer callback
   LogFnCallback m_oLog;

#ifdef DEBUG_CURL
   static std::string s_strCurlTraceLogDirectory;
   mutable std::ofstream m_ofFileCurlTrace;
#endif
   CurlHandle &m_curlHandle;
};

}  // namespace embeddedmz

// Log messages

#define LOG_WARNING_OBJECT_NOT_CLEANED                     \
   "[FTPClient][Warning] Object was freed before calling " \
   "CFTPClient::CleanupSession()."                         \
   " The API session was cleaned though."
#define LOG_ERROR_EMPTY_HOST_MSG "[FTPClient][Error] Empty hostname."
#define LOG_ERROR_CURL_ALREADY_INIT_MSG                        \
   "[FTPClient][Error] Curl session is already initialized ! " \
   "Use CleanupSession() to clean the present one."
#define LOG_ERROR_CURL_NOT_INIT_MSG                       \
   "[FTPClient][Error] Curl session is not initialized !" \
   " Use InitSession() before."
#define LOG_ERROR_CURL_REMOVE_FORMAT "[FTPClient][Error] Unable to remove file %s (Error = %d | %s)."
#define LOG_ERROR_CURL_VERIFYURL_FORMAT                                        \
   "[FTPClient][Error] Unable to connect to the remote folder %s (Error = %d " \
   "| %s)."
#define LOG_ERROR_CURL_FILETIME_FORMAT "[FTPClient][Error] Unable to get file %s's info (Error = %d | %s)."
#define LOG_ERROR_CURL_GETFILE_FORMAT "[FTPClient][Error] Unable to import remote File %s/%s (Error = %d | %s)."
#define LOG_ERROR_CURL_UPLOAD_FORMAT "[FTPClient][Error] Unable to upload file %s (Error = %d | %s)."
#define LOG_ERROR_CURL_FILELIST_FORMAT                                       \
   "[FTPClient][Error] Unable to connect to remote folder %s (Error = %d | " \
   "%s)."
#define LOG_ERROR_CURL_GETWILD_FORMAT "[FTPClient][Error] Unable to import elements %s/%s (Error = %d | %s)."
#define LOG_ERROR_CURL_GETWILD_REC_FORMAT "[FTPClient][Error] Encountered a problem while importing %s to %s."
#define LOG_ERROR_CURL_MKDIR_FORMAT "[FTPClient][Error] Unable to create directory %s (Error = %d | %s)."
#define LOG_ERROR_CURL_RMDIR_FORMAT "[FTPClient][Error] Unable to remove directory %s (Error = %d | %s)."

#define LOG_ERROR_FILE_UPLOAD_FORMAT                     \
   "[FTPClient][Error] Unable to open local file %s in " \
   "CFTPClient::UploadFile()."
#define LOG_ERROR_FILE_GETFILE_FORMAT                    \
   "[FTPClient][Error] Unable to open local file %s in " \
   "CFTPClient::DownloadFile()."
#define LOG_ERROR_DIR_GETWILD_FORMAT                               \
   "[FTPClient][Error] %s is not a directory or it doesn't exist " \
   "in CFTPClient::DownloadWildcard()."

#endif
