#include "gtest/gtest.h"   // Google Test Framework
#include "test_utils.h"    // Helpers for tests

// Test subject (SUT)
#include "FTPClient.h"

#define PRINT_LOG [](const std::string& strLogMsg) { std::cout << strLogMsg << std::endl;  }

// Test parameters
extern bool FTP_TEST_ENABLED;
extern bool SFTP_TEST_ENABLED;
extern bool HTTP_PROXY_TEST_ENABLED;

extern std::string CURL_LOG_FOLDER;
extern std::string SSL_CERT_FILE;
extern std::string SSL_KEY_FILE;
extern std::string SSL_KEY_PWD;

extern std::string FTP_SERVER;
extern unsigned    FTP_SERVER_PORT;
extern std::string FTP_USERNAME;
extern std::string FTP_PASSWORD;
extern std::string FTP_REMOTE_FILE;
extern std::string FTP_REMOTE_UPLOAD_FOLDER;
extern std::string FTP_REMOTE_DOWNLOAD_FOLDER;

extern std::string SFTP_SERVER;
extern unsigned    SFTP_SERVER_PORT;
extern std::string SFTP_USERNAME;
extern std::string SFTP_PASSWORD;
extern std::string SFTP_REMOTE_FILE;
extern std::string SFTP_REMOTE_UPLOAD_FOLDER;
extern std::string SFTP_REMOTE_DOWNLOAD_FOLDER;

extern std::string PROXY_SERVER;
extern std::string PROXY_SERVER_FAKE;

extern std::mutex g_mtxConsoleMutex;

namespace
{
// fixture for FTP tests
class FTPClientTest : public ::testing::Test
{
protected:
   std::unique_ptr<CFTPClient> m_pFTPClient;

   FTPClientTest() : m_pFTPClient(nullptr)
   {
      #ifdef DEBUG_CURL
      CFTPClient::SetCurlTraceLogDirectory(CURL_LOG_FOLDER);
      #endif
   }

   virtual ~FTPClientTest()
   {

   }

   virtual void SetUp()
   {
      m_pFTPClient.reset(new CFTPClient(PRINT_LOG));
      m_pFTPClient->InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD);
   }

   virtual void TearDown()
   {
      if (m_pFTPClient.get() != nullptr)
      {
         m_pFTPClient->CleanupSession();
         m_pFTPClient.reset();
      }
   }
};

// Unit tests

// Tests without a fixture (testing setters/getters and init./cleanup session)
TEST(FTPClient, TestSession)
{
   CFTPClient FTPClient(PRINT_LOG);

   EXPECT_TRUE(FTPClient.GetUsername().empty());
   EXPECT_TRUE(FTPClient.GetPassword().empty());
   EXPECT_TRUE(FTPClient.GetURL().empty());
   EXPECT_TRUE(FTPClient.GetProxy().empty());
   EXPECT_TRUE(FTPClient.GetSSLCertFile().empty());
   EXPECT_TRUE(FTPClient.GetSSLKeyFile().empty());
   EXPECT_TRUE(FTPClient.GetSSLKeyPwd().empty());

   EXPECT_FALSE(FTPClient.GetActive());
   EXPECT_FALSE(FTPClient.GetNoSignal());

   EXPECT_EQ(0, FTPClient.GetTimeout());

   EXPECT_TRUE(FTPClient.GetCurlPointer() == nullptr);

   EXPECT_EQ(CFTPClient::SettingsFlag::ALL_FLAGS, FTPClient.GetSettingsFlags());
   EXPECT_EQ(CFTPClient::FTP_PROTOCOL::FTP, FTPClient.GetProtocol());

   ASSERT_TRUE( FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD,
                                          CFTPClient::FTP_PROTOCOL::SFTP,
                                          CFTPClient::ENABLE_LOG) );

   EXPECT_EQ(CFTPClient::ENABLE_LOG, FTPClient.GetSettingsFlags());
   EXPECT_EQ(CFTPClient::FTP_PROTOCOL::SFTP, FTPClient.GetProtocol());

   EXPECT_TRUE(FTPClient.GetCurlPointer() != nullptr);
   FTPClient.SetProxy("my_proxy");
   FTPClient.SetSSLCertFile("file.cert");
   FTPClient.SetSSLKeyFile("key.key");
   FTPClient.SetSSLKeyPassword("passphrase");
   FTPClient.SetTimeout(10);
   FTPClient.SetActive(true);
   FTPClient.SetNoSignal(true);

   EXPECT_TRUE(FTPClient.GetActive());
   EXPECT_TRUE(FTPClient.GetNoSignal());

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

TEST(FTPClient, TestDoubleInitializingSession)
{
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_TRUE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, "amine", FTP_PASSWORD));
   EXPECT_FALSE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD));
   EXPECT_TRUE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestDoubleCleanUp)
{
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_TRUE(FTPClient.InitSession(FTP_SERVER, FTP_SERVER_PORT, FTP_USERNAME, FTP_PASSWORD));
   EXPECT_TRUE(FTPClient.CleanupSession());
   EXPECT_FALSE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestCleanUpWithoutInit)
{
   CFTPClient FTPClient(PRINT_LOG);

   ASSERT_FALSE(FTPClient.CleanupSession());
}

TEST(FTPClient, TestMultithreading)
{
   const char* arrDataArray[3] = { "Thread 1", "Thread 2", "Thread 3" };
   unsigned uInitialCount = CFTPClient::GetCurlSessionCount();

   auto ThreadFunction = [](const char* pszThreadName)
   {
      CFTPClient FTPClient([](const std::string& strMsg) { std::cout << strMsg << std::endl; });
      if (pszThreadName != nullptr)
      {
         g_mtxConsoleMutex.lock();
         std::cout << pszThreadName << std::endl;
         g_mtxConsoleMutex.unlock();
      }
   };

   std::thread FirstThread(ThreadFunction, arrDataArray[0]);
   std::thread SecondThread(ThreadFunction, arrDataArray[1]);
   std::thread ThirdThread(ThreadFunction, arrDataArray[2]);

   // synchronize threads
   FirstThread.join();                 // pauses until first finishes
   SecondThread.join();                // pauses until second finishes
   ThirdThread.join();                 // pauses until third finishes

   ASSERT_EQ(uInitialCount, CFTPClient::GetCurlSessionCount());
}

TEST_F(FTPClientTest, TestDownloadFile)
{
   if (FTP_TEST_ENABLED)
   {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestDLProgressCallback);
      
      EXPECT_TRUE(m_pFTPClient->DownloadFile("downloaded_" + FTP_REMOTE_FILE, FTP_REMOTE_FILE));
      
      /* to properly show the progress bar */
      std::cout << std::endl;
      
      /* TODO : we can check the SHA1 of the downloaded file with a value provided in the INI file */

      /* delete test file */
      EXPECT_TRUE(remove(("downloaded_" + FTP_REMOTE_FILE).c_str()) == 0);
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(FTPClientTest, TestDownloadInexistantFile)
{
   if (FTP_TEST_ENABLED)
   {
      // inexistant file, expect failure
      EXPECT_FALSE(m_pFTPClient->DownloadFile("downloaded_inexistent_file.xxx", "inexistent_file.xxx"));
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestFileInfo)
{
   CFTPClient::FileInfo ResFileInfo = { 0, 0.0 };

   if (FTP_TEST_ENABLED)
   {
      ASSERT_TRUE(m_pFTPClient->Info(FTP_REMOTE_FILE, ResFileInfo));
      EXPECT_GT(ResFileInfo.dFileSize, 0);
      EXPECT_GT(ResFileInfo.tFileMTime, 0);

      /* TODO : we can check the mtime of the remote file with an epoch value provided in the INI file */
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Check for failure
TEST_F(FTPClientTest, TestGetInexistantFileInfo)
{
   CFTPClient::FileInfo ResFileInfo = { 0, 0.0 };

   if (FTP_TEST_ENABLED)
   {
      EXPECT_FALSE(m_pFTPClient->Info("inexistent_file.xxx", ResFileInfo));
      EXPECT_EQ(ResFileInfo.dFileSize, 0.0);
      EXPECT_EQ(ResFileInfo.tFileMTime, 0);
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Upload Tests
// check return code
TEST_F(FTPClientTest, TestUploadAndRemoveFile)
{
   if (FTP_TEST_ENABLED)
   {
      // to display a beautiful progress bar on console
      m_pFTPClient->SetProgressFnCallback(m_pFTPClient.get(), &TestUPProgressCallback);

      std::ostringstream ssTimestamp;
      TimeStampTest(ssTimestamp);

      // create dummy test file
      std::ofstream ofTestUpload("test_upload.txt");
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));

      ofTestUpload << "Unit Test TestUploadFile executed on " + ssTimestamp.str() + "\n"
                    + "This file is uploaded via FTPClient-C++ API.\n"
                    + "If this file exists, that means that the unit test is passed.\n";
      ASSERT_TRUE(static_cast<bool>(ofTestUpload));
      ofTestUpload.close();

      // Upload file and create a directory "upload_test"
      ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt",
                  FTP_REMOTE_UPLOAD_FOLDER + "upload_test/test_upload.txt",
                  true));
      
      /* to properly show the progress bar */
      std::cout << std::endl;

      // Upload file
      ASSERT_TRUE(m_pFTPClient->UploadFile("test_upload.txt",
                  FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));
      
      /* to properly show the progress bar */
      std::cout << std::endl;

      // Remove file
      ASSERT_TRUE(m_pFTPClient->RemoveFile(FTP_REMOTE_UPLOAD_FOLDER + "test_upload.txt"));

      // delete test file
      EXPECT_TRUE(remove("test_upload.txt") == 0);

      /* TODO : we can download the uploaded file and check if its content is equal to the original one 
         we can use the helper function AreFilesEqual()
      */
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestUploadFailure)
{
   if (FTP_TEST_ENABLED)
   {
      // upload of an inexistant file must fail.
      ASSERT_FALSE(m_pFTPClient->UploadFile("inexistant_file.doc",
                   FTP_REMOTE_UPLOAD_FOLDER + "inexistant_file.doc"));
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestList)
{
   if (FTP_TEST_ENABLED)
   {
      std::string strList;

      /* list root directory */
      EXPECT_TRUE(m_pFTPClient->List("/", strList, false));
      EXPECT_FALSE(strList.empty());
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestWildcardedURL)
{
   #ifdef LINUX
   mkdir("Wildcard", ACCESSPERMS);
   #else
   _mkdir("Wildcard");
   #endif

   if (FTP_TEST_ENABLED)
   {
      // all the content of FTP_REMOTE_DOWNLOAD_FOLDER/*
      EXPECT_TRUE(m_pFTPClient->DownloadWildcard("Wildcard", FTP_REMOTE_DOWNLOAD_FOLDER));

      /* TODO : check the existence of the downloaded items (Helpers_cpp project can be useful) */


      /* TODO : delete recusively the files (Helpers_cpp project can be useful) */

   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// check for failure
TEST_F(FTPClientTest, TestWildcardedURLFailure)
{
   if (FTP_TEST_ENABLED)
   {
      EXPECT_FALSE(m_pFTPClient->DownloadWildcard("InexistentDir", "*"));
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

TEST_F(FTPClientTest, TestCreateAndRemoveDirectory)
{
   if (FTP_TEST_ENABLED)
   {
      ASSERT_TRUE(m_pFTPClient->CreateDir(FTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
      EXPECT_TRUE(m_pFTPClient->RemoveDir(FTP_REMOTE_UPLOAD_FOLDER + "bookmarks"));
   }
   else
      std::cout << "FTP tests are disabled !" << std::endl;
}

// Proxy Tests
TEST_F(FTPClientTest, TestProxyList)
{
   if (HTTP_PROXY_TEST_ENABLED && FTP_TEST_ENABLED)
   {
      std::string strList;
      
      m_pFTPClient->SetProxy(PROXY_SERVER);

      /* to avoid long waitings if something goes wrong */
      m_pFTPClient->SetTimeout(10);
      
      EXPECT_TRUE(m_pFTPClient->List("/", strList));
      EXPECT_FALSE(strList.empty());
   }
   else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

// check for failure
TEST_F(FTPClientTest, TestInexistantProxy)
{
   if (HTTP_PROXY_TEST_ENABLED && FTP_TEST_ENABLED)
   {
      std::string strList;
      
      /* fake proxy address */
      m_pFTPClient->SetProxy(PROXY_SERVER_FAKE);

      /* to avoid long waitings - 5 seconds timeout */
      m_pFTPClient->SetTimeout(5);

      EXPECT_FALSE(m_pFTPClient->List("/", strList, true));
      EXPECT_TRUE(strList.empty());
   }
   else
      std::cout << "HTTP Proxy tests are disabled !" << std::endl;
}

// TODO : SFTP tests to write...

} // namespace

int main(int argc, char **argv)
{
   if (argc > 1 && GlobalTestInit(argv[1])) // loading test parameters from the INI file...
   {
      ::testing::InitGoogleTest(&argc, argv);
      int iTestResults = RUN_ALL_TESTS();

      ::GlobalTestCleanUp();

      return iTestResults;
   }
   else
   {
      std::cerr << "[ERROR] Encountered an error while loading test parameters from provided INI file !" << std::endl;
      return 1;
   }
}
