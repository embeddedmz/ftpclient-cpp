#include "gtest/gtest.h"  // Google Test Framework
#include "test_utils.h"   // Helpers for tests

// Test subject (SUT)
#include "FTPClient.h"

#define PRINT_LOG [](const std::string& strLogMsg) { std::cout << strLogMsg << std::endl; }

// Test parameters
extern bool FTP_TEST_ENABLED;
extern bool SFTP_TEST_ENABLED;
extern bool HTTP_PROXY_TEST_ENABLED;

extern std::string CURL_LOG_FOLDER;
extern std::string SSL_CERT_FILE;
extern std::string SSL_KEY_FILE;
extern std::string SSL_KEY_PWD;

extern std::string FTP_SERVER;
extern unsigned FTP_SERVER_PORT;
extern std::string FTP_USERNAME;
extern std::string FTP_PASSWORD;
extern std::string FTP_REMOTE_FILE;
extern std::string FTP_REMOTE_FILE_SHA1SUM;
extern std::string FTP_REMOTE_UPLOAD_FOLDER;
extern std::string FTP_REMOTE_DOWNLOAD_FOLDER;

extern std::string SFTP_SERVER;
extern unsigned SFTP_SERVER_PORT;
extern std::string SFTP_USERNAME;
extern std::string SFTP_PASSWORD;
extern std::string SFTP_REMOTE_FILE;
extern std::string SFTP_REMOTE_FILE_SHA1SUM;
extern std::string SFTP_REMOTE_UPLOAD_FOLDER;
extern std::string SFTP_REMOTE_DOWNLOAD_FOLDER;

extern std::string PROXY_SERVER;
extern std::string PROXY_SERVER_FAKE;

extern std::mutex g_mtxConsoleMutex;

namespace embeddedmz {
// fixture for FTP tests
class FTPClientTest : public ::testing::Test {
  protected:
   std::unique_ptr<CFTPClient> m_pFTPClient;

   FTPClientTest() : m_pFTPClient(nullptr) {
#ifdef DEBUG_CURL
      CFTPClient::SetCurlTraceLogDirectory(CURL_LOG_FOLDER);
#endif
   }

   virtual ~FTPClientTest() {}

   virtual void SetUp() {
      m_pFTPClient.reset(new CFTPClient(PRINT_LOG));
      m_pFTPClient->InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD,
          CFTPClient::FTP_PROTOCOL::FTP, CFTPClient::SettingsFlag::ENABLE_LOG);

      // needed to avoid some rare test failure - though, it seems that libcurl timeout are not working properly...
      //m_pFTPClient->SetTimeout(500);
   }

   virtual void TearDown() {
      if (m_pFTPClient.get() != nullptr) {
         m_pFTPClient->CleanupSession();
         m_pFTPClient.reset();
      }
   }
};

// fixture for SFTP tests
class SFTPClientTest : public ::testing::Test {
  protected:
   std::unique_ptr<CFTPClient> m_pSFTPClient;

   SFTPClientTest() : m_pSFTPClient(nullptr) {
#ifdef DEBUG_CURL
      CFTPClient::SetCurlTraceLogDirectory(CURL_LOG_FOLDER);
#endif
   }

   virtual ~SFTPClientTest() {}

   virtual void SetUp() {
      m_pSFTPClient.reset(new CFTPClient(PRINT_LOG));
      m_pSFTPClient->InitSession(SFTP_SERVER, SFTP_SERVER_PORT, SFTP_USERNAME, SFTP_PASSWORD, CFTPClient::FTP_PROTOCOL::SFTP,
                                 CFTPClient::SettingsFlag::ENABLE_LOG);
      m_pSFTPClient->SetInsecure(true);

      // needed to avoid some rare test failure - timeout seems to not working
      //m_pSFTPClient->SetTimeout(500);
   }

   virtual void TearDown() {
      if (m_pSFTPClient.get() != nullptr) {
         m_pSFTPClient->CleanupSession();
         m_pSFTPClient.reset();
      }
   }
};

// Unit tests

// Tests without a fixture (testing setters/getters and init./cleanup session)
TEST(FTPClient, TestSession) {
   CFTPClient FTPClient(PRINT_LOG);

   EXPECT_TRUE(FTPClient.GetUsername().empty());
   EXPECT_TRUE(FTPClient.GetPassword().empty());
   EXPECT_TRUE(FTPClient.GetURL().empty());
   EXPECT_TRUE(FTPClient.GetProxy().empty());
   EXPECT_TRUE(FTPClient.GetSSLCertFile().empty());
   EXPECT_TRUE(FTPClient.GetSSLKeyFile().empty());
   EXPECT_TRUE(FTPClient.GetSSLKeyPwd().empty());

   EXPECT_FALSE(FTPClient.GetActive());
   EXPECT_TRUE(FTPClient.GetNoSignal());

   EXPECT_EQ(0, FTPClient.GetTimeout());

   EXPECT_TRUE(FTPClient.GetCurlPointer() == nullptr);

   EXPECT_EQ(CFTPClient::SettingsFlag::NO_FLAGS, FTPClient.GetSettingsFlags());
   EXPECT_EQ(CFTPClient::FTP_PROTOCOL::FTP, FTPClient.GetProtocol());

   ASSERT_TRUE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD, CFTPClient::FTP_PROTOCOL::SFTP,
                                     CFTPClient::ENABLE_LOG | CFTPClient::ENABLE_SSH_AGENT));

   EXPECT_EQ(CFTPClient::ENABLE_LOG | CFTPClient::ENABLE_SSH_AGENT, FTPClient.GetSettingsFlags());
   EXPECT_EQ(CFTPClient::FTP_PROTOCOL::SFTP, FTPClient.GetProtocol());

   EXPECT_TRUE(FTPClient.GetCurlPointer() != nullptr);
   FTPClient.SetProxy("my_proxy");
   FTPClient.SetSSLCertFile("file.cert");
   FTPClient.SetSSLKeyFile("key.key");
   FTPClient.SetSSLKeyPassword("passphrase");
   FTPClient.SetTimeout(10);
   FTPClient.SetActive(true);
   FTPClient.SetNoSignal(false);

   EXPECT_TRUE(FTPClient.GetActive());
   EXPECT_FALSE(FTPClient.GetNoSignal());

   EXPECT_STREQ(FTP_SERVER.c_str(), FTPClient.GetURL().c_str());
   EXPECT_EQ(FTP_SERVER_PORT, FTPClient.GetPort());
   EXPECT_STREQ(FTP_USERNAME.c_str(), FTPClient.GetUsername().c_str());
   EXPECT_STREQ(FTP_PASSWORD.c_str(), FTPClient.GetPassword().c_str());

   EXPECT_STREQ("http://my_proxy", FTPClient.GetProxy().c_str());
   EXPECT_STREQ("file.cert", FTPClient.GetSSLCertFile().c_str());
   EXPECT_STREQ("key.key", FTPClient.GetSSLKeyFile().c_str());
   EXPECT_STREQ("passphrase", FTPClient.GetSSLKeyPwd().c_str());

   EXPECT_EQ(10, FTPClient.GetTimeout());

   FTPClient.SetProgressFnCallback(reinterpret_cast<void*>(0xFFFFFFFF), &TestDLProgressCallback);
   EXPECT_EQ(&TestDLProgressCallback, *FTPClient.GetProgressFnCallback());
   EXPECT_EQ(reinterpret_cast<void*>(0xFFFFFFFF), FTPClient.GetProgressFnCallbackOwner());

   EXPECT_TRUE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestDoubleInitializingSession) {
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_TRUE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, "amine", FTP_PASSWORD));
   EXPECT_FALSE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD));
   EXPECT_TRUE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestDoubleCleanUp) {
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_TRUE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD));
   EXPECT_TRUE(FTPClient.CleanupSession());
   EXPECT_FALSE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestCleanUpWithoutInit) {
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_FALSE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestMultithreading) {
   const char* arrDataArray[3] = {"Thread 1", "Thread 2", "Thread 3"};

   auto ThreadFunction = [](const char* pszThreadName) {
      CFTPClient FTPClient([](const std::string& strMsg) { std::cout << strMsg << std::endl; });
      if (pszThreadName != nullptr) {
         std::unique_lock<std::mutex> lock{g_mtxConsoleMutex};
         std::cout << pszThreadName << std::endl;
      }
   };

   std::thread FirstThread(ThreadFunction, arrDataArray[0]);
   std::thread SecondThread(ThreadFunction, arrDataArray[1]);
   std::thread ThirdThread(ThreadFunction, arrDataArray[2]);

   // synchronize threads
   FirstThread.join();   // pauses until first finishes
   SecondThread.join();  // pauses until second finishes
   ThirdThread.join();   // pauses until third finishes
}

TEST_F(FTPClientTest, TestDownloadFile) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestDLProgressCallback);

      #ifdef WINDOWS
      // Convert file name from ANSI to UTF8
      std::string remoteFileUtf8 = CFTPClient::AnsiToUtf8(FTP_REMOTE_FILE);
      #else
      std::string remoteFileUtf8 = FTP_REMOTE_FILE;
      #endif

      ASSERT_TRUE(m_pFTPClient->DownloadFile("downloaded_file", remoteFileUtf8));

      /* to properly show the progress bar */
      std::cout << std::endl;

      /* check the SHA1 sum of the downloaded file if possible */
      if (!FTP_REMOTE_FILE_SHA1SUM.empty()) {
         std::string ret = sha1sum("downloaded_file");
         std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
         EXPECT_TRUE(FTP_REMOTE_FILE_SHA1SUM == ret);
      }

      /* delete test file */
      EXPECT_TRUE(remove("downloaded_file") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

#ifdef WINDOWS
TEST_F(FTPClientTest, TestSaveFileNameWithAccents) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestDLProgressCallback);
      
      // Convert file name from ANSI to UTF8
      std::string remoteFileUtf8 = CFTPClient::AnsiToUtf8(FTP_REMOTE_FILE);
      std::string localFileNameUtf8 = CFTPClient::AnsiToUtf8("fichier_téléchargé");

      ASSERT_TRUE(m_pFTPClient->DownloadFile(localFileNameUtf8, remoteFileUtf8));

      /* to properly show the progress bar */
      std::cout << std::endl;

      /* check the SHA1 sum of the downloaded file if possible */
      if (!FTP_REMOTE_FILE_SHA1SUM.empty()) {
         std::string ret = sha1sum("fichier_téléchargé");
         std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
         EXPECT_TRUE(FTP_REMOTE_FILE_SHA1SUM == ret);
      }

      /* delete test file */
      EXPECT_TRUE(remove("fichier_téléchargé") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}
#endif

TEST_F(FTPClientTest, TestDownloadFileToMem) {
   if (FTP_TEST_ENABLED) {
      // stores file content
      std::vector<char> output;
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestDLProgressCallback);

      #ifdef WINDOWS
      // Convert file name from ANSI to UTF8
      std::string remoteFileUtf8 = CFTPClient::AnsiToUtf8(FTP_REMOTE_FILE);
      #else
      std::string remoteFileUtf8 = FTP_REMOTE_FILE;
      #endif

      ASSERT_TRUE(m_pFTPClient->DownloadFile(remoteFileUtf8, output));

      /* to properly show the progress bar */
      std::cout << std::endl;

      /* check the SHA1 sum of the downloaded file if possible */
      if (!FTP_REMOTE_FILE_SHA1SUM.empty()) {
         std::string ret = sha1sum(output);
         std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
         EXPECT_TRUE(FTP_REMOTE_FILE_SHA1SUM == ret);
      }
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// this test was created when I was using a buggy FTP server aka FileZilla
TEST_F(FTPClientTest, TestDownloadFile10Times) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestDLProgressCallback);

      #ifdef WINDOWS
         // Convert file name from ANSI to UTF8
         std::string remoteFileUtf8 = CFTPClient::AnsiToUtf8(FTP_REMOTE_FILE);
      #else
         std::string remoteFileUtf8 = FTP_REMOTE_FILE;
      #endif
      
      for (unsigned i = 0; i < 10; ++i) {
         ASSERT_TRUE(m_pFTPClient->DownloadFile("downloaded_file", remoteFileUtf8));

         /* to properly show the progress bar */
         std::cout << std::endl;

         /* check the SHA1 sum of the downloaded file if possible */
         if (!FTP_REMOTE_FILE_SHA1SUM.empty()) {
            std::string ret = sha1sum("downloaded_file");
            std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
            EXPECT_TRUE(FTP_REMOTE_FILE_SHA1SUM == ret);
         }
      }

      /* delete test file */
      EXPECT_TRUE(remove("downloaded_file") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(FTPClientTest, TestDownloadInexistantFile) {
   if (FTP_TEST_ENABLED) {
      // inexistant file, expect failure
      ASSERT_FALSE(m_pFTPClient->DownloadFile("downloaded_inexistent_file.xxx", "inexistent_file.xxx"));
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestFileInfo) {
   CFTPClient::FileInfo ResFileInfo = {0, 0.0};

   if (FTP_TEST_ENABLED) {
#ifdef WINDOWS
          // Convert file name from ANSI to UTF8
      std::string remoteFileUtf8 = CFTPClient::AnsiToUtf8(FTP_REMOTE_FILE);
#else
      std::string remoteFileUtf8 = FTP_REMOTE_FILE;
#endif

      ASSERT_TRUE(m_pFTPClient->Info(remoteFileUtf8, ResFileInfo));
      EXPECT_GT(ResFileInfo.dFileSize, 0);
      EXPECT_GT(ResFileInfo.tFileMTime, 0);

      /* TODO : we can check the mtime of the remote file with an epoch value provided in the INI file */
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(FTPClientTest, TestGetInexistantFileInfo) {
   CFTPClient::FileInfo ResFileInfo = {0, 0.0};

   if (FTP_TEST_ENABLED) {
      ASSERT_FALSE(m_pFTPClient->Info("inexistent_file.xxx", ResFileInfo));
      EXPECT_EQ(ResFileInfo.dFileSize, 0.0);
      EXPECT_EQ(ResFileInfo.tFileMTime, 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// TODO : this unit test can't be executed elsewhere
#if 0
TEST_F(FTPClientTest, TestAppend/*AndRemoveFile*/) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      // Upload file
      ASSERT_TRUE(m_pFTPClient->UploadFile("C:\\Users\\mmzoughi\\Desktop\\test_upload_part1.txt", FTP_REMOTE_UPLOAD_FOLDER + "test_append.txt"));

      std::cout << std::endl;

      ASSERT_TRUE(m_pFTPClient->AppendFile("C:\\Users\\mmzoughi\\Desktop\\test_upload.txt", 84, FTP_REMOTE_UPLOAD_FOLDER + "test_append.txt"));

      std::cout << std::endl;

      // Download the uploaded file into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pFTPClient->DownloadFile(FTP_REMOTE_UPLOAD_FOLDER + "test_append.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string expectedSha1Sum = sha1sum("C:\\Users\\mmzoughi\\Desktop\\test_upload.txt");
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      //ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      // delete test file
      //EXPECT_TRUE(remove("test_append.txt") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}
#endif

// Upload Tests
// check return code
TEST_F(FTPClientTest, TestUploadAndRemoveFile) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy test file
      std::ofstream ofTestUpload("test_upload.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n" +
                          "This file is uploaded via FTPClient-C++ API.\n" +
                          "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      // Upload file and create a directory "upload_test"
      ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt", FTP_REMOTE_UPLOAD_FOLDER + "upload_test/test_upload.txt", true));

      /* to properly show the progress bar */
      std::cout << std::endl;

      // Upload file
      ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt", FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      std::cout << std::endl;

      // Download the uploaded file into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pFTPClient->DownloadFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string expectedSha1Sum = sha1sum("test_upload.txt");
         std::string resultSha1Sum = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      // delete test file
      EXPECT_TRUE(remove("test_upload.txt") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestUploadStreamAndRemove) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy string stream
      std::stringstream ofTestUpload;
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test 'TestUploadStreamAndRemove' executed on " + ssTimestamp.str() + "\n" +
                      "This file is uploaded via FTPClient-C++ API.\n" +
                      "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      // Upload steam
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ASSERT_TRUE(m_pFTPClient->UploadFile(ofTestUpload, SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt"));

      std::cout << std::endl;

      // Download the uploaded stream into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pFTPClient->DownloadFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string contentStr = ofTestUpload.str();
         std::vector<char> contentBytes(contentStr.begin(), contentStr.end());
         std::string expectedSha1Sum = sha1sum(contentBytes);
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      ASSERT_TRUE(m_pFTPClient->RemoveFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt"));
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

#ifdef WINDOWS
TEST_F(FTPClientTest, TestUploadFileNameWithAccents) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // Convert file name from ANSI to UTF8
      std::string fileNameUtf8 = CFTPClient::AnsiToUtf8("fichier_à_téléverser.txt");

      // create dummy test file
      std::ofstream ofTestUpload("fichier_à_téléverser.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n" +
                          "This file is uploaded via FTPClient-C++ API.\n" +
                          "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      // Upload file and create a directory "upload_test"
      ASSERT_TRUE(m_pFTPClient->UploadFile(fileNameUtf8, FTP_REMOTE_UPLOAD_FOLDER + "upload_test_accents/" + fileNameUtf8, true));

      /* to properly show the progress bar */
      std::cout << std::endl;

      // Upload file
      ASSERT_TRUE(m_pFTPClient->UploadFile(fileNameUtf8, FTP_REMOTE_UPLOAD_FOLDER + fileNameUtf8));

      std::cout << std::endl;

      // Download the uploaded file into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pFTPClient->DownloadFile(FTP_REMOTE_UPLOAD_FOLDER + fileNameUtf8, uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string expectedSha1Sum = sha1sum("fichier_à_téléverser.txt");
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + fileNameUtf8));

      // delete test file
      EXPECT_TRUE(remove("fichier_à_téléverser.txt") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}
#endif

TEST_F(FTPClientTest, TestUploadAndRemoveFile10Times) {
   if (FTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy test file
      std::ofstream ofTestUpload("test_upload.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n" +
                          "This file is uploaded via FTPClient-C++ API.\n" +
                          "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      for (unsigned i = 0; i < 10; ++i) {
          // Upload file and create a directory "upload_test"
          ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt", FTP_REMOTE_UPLOAD_FOLDER + "upload_test/test_upload.txt", true));

          /* to properly show the progress bar */
          std::cout << std::endl;

          // Upload file
          ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt", FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

          std::cout << std::endl;

          // Download the uploaded file into a vector of bytes
          {
             std::vector<char> uploadedFileBytes;
             EXPECT_TRUE(m_pFTPClient->DownloadFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt", uploadedFileBytes));

             std::cout << std::endl;

             /* check the SHA1 sum of the uploaded file */
             std::string expectedSha1Sum = sha1sum("test_upload.txt");
             std::string resultSha1Sum = sha1sum(uploadedFileBytes);

             EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
          }

          // Remove file
          ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));
      }

      // delete test file
      EXPECT_TRUE(remove("test_upload.txt") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestUploadFailure) {
   if (FTP_TEST_ENABLED) {
      // upload of an inexistant file must fail.
      ASSERT_FALSE(m_pFTPClient->UploadFile("inexistant_file.doc", FTP_REMOTE_UPLOAD_FOLDER + "inexistant_file.doc"));
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestList) {
   if (FTP_TEST_ENABLED) {
      std::string strList;

      /* list root directory */
      ASSERT_TRUE(m_pFTPClient->List("/", strList, false));
      EXPECT_FALSE(strList.empty());
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestWildcardedURL) {
#ifdef LINUX
   mkdir("Wildcard", ACCESSPERMS);
#else
   _mkdir("Wildcard");
#endif

   if (FTP_TEST_ENABLED) {
      // all the content of FTP_REMOTE_DOWNLOAD_FOLDER/*
      ASSERT_TRUE(m_pFTPClient->DownloadWildcard("Wildcard", FTP_REMOTE_DOWNLOAD_FOLDER));

      /* TODO : check the existence of the downloaded items (Helpers_cpp project can be useful) */

      /* TODO : delete recusively the files (Helpers_cpp project can be useful) */

   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// check for failure
TEST_F(FTPClientTest, TestWildcardedURLFailure) {
   if (FTP_TEST_ENABLED) {
      ASSERT_FALSE(m_pFTPClient->DownloadWildcard("InexistentDir", "*"));
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestCreateAndRemoveDirectory) {
   if (FTP_TEST_ENABLED) {
      ASSERT_TRUE(m_pFTPClient->CreateDir(FTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
      EXPECT_TRUE(m_pFTPClient->RemoveDir(FTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Proxy Tests
TEST_F(FTPClientTest, TestProxyList) {
   if (HTTP_PROXY_TEST_ENABLED && FTP_TEST_ENABLED) {
      std::string strList;

      m_pFTPClient->SetProxy(PROXY_SERVER);

      ASSERT_TRUE(m_pFTPClient->List("/", strList));
      EXPECT_FALSE(strList.empty());
   } else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

// check for failure
TEST_F(FTPClientTest, TestInexistantProxy) {
   if (HTTP_PROXY_TEST_ENABLED && FTP_TEST_ENABLED) {
      std::string strList;

      /* fake proxy address */
      m_pFTPClient->SetProxy(PROXY_SERVER_FAKE);

      /* to avoid long waitings - 5 seconds timeout */
      m_pFTPClient->SetTimeout(5);

      ASSERT_FALSE(m_pFTPClient->List("/", strList, true));
      EXPECT_TRUE(strList.empty());
   } else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

/* SFTP Unit Tests */
TEST_F(SFTPClientTest, TestDownloadFile) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestDLProgressCallback);

      ASSERT_TRUE(m_pSFTPClient->DownloadFile("downloaded_file", SFTP_REMOTE_FILE));

      /* to properly show the progress bar */
      std::cout << std::endl;

      /* check the SHA1 sum of the downloaded file if possible */
      if (!SFTP_REMOTE_FILE_SHA1SUM.empty()) {
         std::string ret = sha1sum("downloaded_file");
         std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
         EXPECT_TRUE(SFTP_REMOTE_FILE_SHA1SUM == ret);
      }

      /* delete test file */
      EXPECT_TRUE(remove("downloaded_file") == 0);
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestDownloadFileToMem) {
   if (SFTP_TEST_ENABLED) {
      // stores file content
      std::vector<char> output;
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestDLProgressCallback);

      ASSERT_TRUE(m_pSFTPClient->DownloadFile(SFTP_REMOTE_FILE, output));

      /* to properly show the progress bar */
      std::cout << std::endl;

      /* check the SHA1 sum of the downloaded file if possible */
      if (!SFTP_REMOTE_FILE_SHA1SUM.empty()) {
         std::string ret = sha1sum(output);
         std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
         EXPECT_TRUE(SFTP_REMOTE_FILE_SHA1SUM == ret);
      }
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestDownloadFile10Times) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestDLProgressCallback);

      for (unsigned i = 0; i < 10; ++i) {
         ASSERT_TRUE(m_pSFTPClient->DownloadFile("downloaded_file", SFTP_REMOTE_FILE));

         /* to properly show the progress bar */
         std::cout << std::endl;

         /* check the SHA1 sum of the downloaded file if possible */
         if (!SFTP_REMOTE_FILE_SHA1SUM.empty()) {
            std::string ret = sha1sum("downloaded_file");
            std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
            EXPECT_TRUE(SFTP_REMOTE_FILE_SHA1SUM == ret);
         }
      }

      /* delete test file */
      EXPECT_TRUE(remove("downloaded_file") == 0);
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(SFTPClientTest, TestDownloadInexistantFile) {
   if (SFTP_TEST_ENABLED) {
      // inexistant file, expect failure
      ASSERT_FALSE(m_pSFTPClient->DownloadFile("downloaded_inexistent_file.xxx", "inexistent_file.xxx"));
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestFileInfo) {
   CFTPClient::FileInfo ResFileInfo = {0, 0.0};

   if (SFTP_TEST_ENABLED) {
      ASSERT_TRUE(m_pSFTPClient->Info(SFTP_REMOTE_FILE, ResFileInfo));
      EXPECT_GT(ResFileInfo.dFileSize, 0);
      EXPECT_GT(ResFileInfo.tFileMTime, 0);

      /* TODO : we can check the mtime of the remote file with an epoch value provided in the INI file */
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(SFTPClientTest, TestGetInexistantFileInfo) {
   CFTPClient::FileInfo ResFileInfo = {0, 0.0};

   if (SFTP_TEST_ENABLED) {
      ASSERT_FALSE(m_pSFTPClient->Info("inexistent_file.xxx", ResFileInfo));
      EXPECT_EQ(ResFileInfo.dFileSize, 0.0);
      EXPECT_EQ(ResFileInfo.tFileMTime, 0);
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// Upload Tests
// check return code
TEST_F(SFTPClientTest, TestUploadAndRemoveFile) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy test file
      std::ofstream ofTestUpload("test_upload.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n" +
                          "This file is uploaded via FTPClient-C++ API.\n" +
                          "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      // Upload file and create a directory "upload_test"
      ASSERT_TRUE(m_pSFTPClient->UploadFile("test_upload.txt", SFTP_REMOTE_UPLOAD_FOLDER + "upload_test/test_upload.txt", true));

      /* to properly show the progress bar */
      std::cout << std::endl;

      // Upload file
      ASSERT_TRUE(m_pSFTPClient->UploadFile("test_upload.txt", SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      std::cout << std::endl;

      // Download the uploaded file into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pSFTPClient->DownloadFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string expectedSha1Sum = sha1sum("test_upload.txt");
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      ASSERT_TRUE(m_pSFTPClient->RemoveFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      // delete test file
      EXPECT_TRUE(remove("test_upload.txt") == 0);
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestUploadStreamAndRemove) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy string stream
      std::stringstream ofTestUpload;
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test 'TestUploadStreamAndRemove' executed on " + ssTimestamp.str() + "\n" +
                      "This file is uploaded via FTPClient-C++ API.\n" +
                      "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      // Upload steam
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ASSERT_TRUE(m_pSFTPClient->UploadFile(ofTestUpload, SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt"));

      std::cout << std::endl;

      // Download the uploaded stream into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pSFTPClient->DownloadFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string contentStr = ofTestUpload.str();
         std::vector<char> contentBytes(contentStr.begin(), contentStr.end());
         std::string expectedSha1Sum = sha1sum(contentBytes);
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      ASSERT_TRUE(m_pSFTPClient->RemoveFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload_stream.txt"));
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// TODO : this unit test can't be executed elsewhere
#if 0
TEST_F(SFTPClientTest, TestAppend /*AndRemoveFile*/) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestUPProgressCallback);

      // Upload file
      ASSERT_TRUE(
          m_pSFTPClient->UploadFile("C:\\Users\\mmzoughi\\Desktop\\test_upload_part1.txt", SFTP_REMOTE_UPLOAD_FOLDER + "test_append.txt"));

      std::cout << std::endl;

      ASSERT_TRUE(
          m_pSFTPClient->AppendFile("C:\\Users\\mmzoughi\\Desktop\\test_upload.txt", 84, SFTP_REMOTE_UPLOAD_FOLDER + "test_append.txt"));

      std::cout << std::endl;

      // Download the uploaded file into a vector of bytes
      {
         std::vector<char> uploadedFileBytes;
         EXPECT_TRUE(m_pSFTPClient->DownloadFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_append.txt", uploadedFileBytes));

         std::cout << std::endl;

         /* check the SHA1 sum of the uploaded file */
         std::string expectedSha1Sum = sha1sum("C:\\Users\\mmzoughi\\Desktop\\test_upload.txt");
         std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

         EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
      }

      // Remove file
      // ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      // delete test file
      //EXPECT_TRUE(remove("test_append.txt") == 0);
   } else
      std::cout << "FTP tests are disabled !" << std::endl;
}
#endif

TEST_F(SFTPClientTest, TestUploadAndRemoveFile10Times) {
   if (SFTP_TEST_ENABLED) {
      // to display a beautiful progress bar on console
      m_pSFTPClient->SetProgressFnCallback(m_pSFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy test file
      std::ofstream ofTestUpload("test_upload.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n" +
                          "This file is uploaded via FTPClient-C++ API.\n" +
                          "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      for (unsigned i = 0; i < 10; ++i) {
         // Upload file and create a directory "upload_test"
         ASSERT_TRUE(m_pSFTPClient->UploadFile("test_upload.txt", SFTP_REMOTE_UPLOAD_FOLDER + "upload_test/test_upload.txt", true));

         /* to properly show the progress bar */
         std::cout << std::endl;

         // Upload file
         ASSERT_TRUE(m_pSFTPClient->UploadFile("test_upload.txt", SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

         std::cout << std::endl;

         // Download the uploaded file into a vector of bytes
         {
            std::vector<char> uploadedFileBytes;
            EXPECT_TRUE(m_pSFTPClient->DownloadFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt", uploadedFileBytes));

            std::cout << std::endl;

            /* check the SHA1 sum of the uploaded file */
            std::string expectedSha1Sum = sha1sum("test_upload.txt");
            std::string resultSha1Sum   = sha1sum(uploadedFileBytes);

            EXPECT_TRUE(expectedSha1Sum == resultSha1Sum);
         }

         // Remove file
         ASSERT_TRUE(m_pSFTPClient->RemoveFile(SFTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));
      }

      // delete test file
      EXPECT_TRUE(remove("test_upload.txt") == 0);
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestUploadFailure) {
   if (SFTP_TEST_ENABLED) {
      // upload of an inexistant file must fail.
      ASSERT_FALSE(m_pSFTPClient->UploadFile("inexistant_file.doc", SFTP_REMOTE_UPLOAD_FOLDER + "inexistant_file.doc"));
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestList) {
   if (SFTP_TEST_ENABLED) {
      std::string strList;

      /* list root directory */
      ASSERT_TRUE(m_pSFTPClient->List("/", strList, false));
      EXPECT_FALSE(strList.empty());
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// NB : DownloadWildcard doesn't work with SFTP !
TEST_F(SFTPClientTest, TestWildcardedURL) {
#ifdef LINUX
   mkdir("Wildcard", ACCESSPERMS);
#else
   _mkdir("Wildcard");
#endif

   if (SFTP_TEST_ENABLED) {
      // all the content of FTP_REMOTE_DOWNLOAD_FOLDER/*
      ASSERT_TRUE(m_pSFTPClient->DownloadWildcard("Wildcard", SFTP_REMOTE_DOWNLOAD_FOLDER));

      /* TODO : check the existence of the downloaded items (Helpers_cpp project can be useful) */

      /* TODO : delete recusively the files (Helpers_cpp project can be useful) */

   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

// check for failure - DownloadWildcard doesn't work with SFTP !
TEST_F(SFTPClientTest, TestWildcardedURLFailure) {
   if (SFTP_TEST_ENABLED) {
      ASSERT_FALSE(m_pSFTPClient->DownloadWildcard("InexistentDir", "*"));
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

TEST_F(SFTPClientTest, TestCreateAndRemoveDirectory) {
   if (SFTP_TEST_ENABLED) {
      ASSERT_TRUE(m_pSFTPClient->CreateDir(SFTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
      EXPECT_TRUE(m_pSFTPClient->RemoveDir(SFTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
   } else
      std::cout << "SFTP tests are disabled !" << std::endl;
}

}  // namespace embeddedmz

int main(int argc, char** argv) {
   if (argc > 1 && GlobalTestInit(argv[1]))  // loading test parameters from the INI file...
   {
      ::testing::InitGoogleTest(&argc, argv);
      int iTestResults = RUN_ALL_TESTS();

      ::GlobalTestCleanUp();

      return iTestResults;
   } else {
      std::cerr << "[ERROR] Encountered an error while loading test parameters from provided INI file !" << std::endl;
      return 1;
   }
}
