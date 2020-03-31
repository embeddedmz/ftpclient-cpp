#ifndef INCLUDE_CURLHANDLE_H_
#define INCLUDE_CURLHANDLE_H_

namespace embeddedmz {

class CurlHandle {
  public:
   static CurlHandle &instance();

   CurlHandle(CurlHandle const &) = delete;
   CurlHandle(CurlHandle &&)      = delete;

   CurlHandle &operator=(CurlHandle const &) = delete;
   CurlHandle &operator=(CurlHandle &&) = delete;

   ~CurlHandle();

  private:
   CurlHandle();
};

}  // namespace embeddedmz

#endif
