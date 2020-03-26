#include "CurlHandle.h"

#include <curl/curl.h>

namespace embeddedmz {

   CurlHandle::CurlHandle(){
     curl_global_init(CURL_GLOBAL_ALL);
   }

   CurlHandle::~CurlHandle(){
     curl_global_cleanup();
   }   

   CurlHandle& CurlHandle::instnace(){
     static CurlHandle inst{};
     return inst;
   }

}
