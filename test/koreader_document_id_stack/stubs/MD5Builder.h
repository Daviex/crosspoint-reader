#pragma once

#include <openssl/evp.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "Fixture.h"

// Host adapter for the Arduino MD5Builder surface used by the real document-ID
// implementation. OpenSSL supplies the independent digest, not production code.
class MD5Builder {
 public:
  MD5Builder() : context(EVP_MD_CTX_new()) {
    if (!context) throw std::runtime_error("EVP_MD_CTX_new failed");
  }
  ~MD5Builder() { EVP_MD_CTX_free(context); }
  MD5Builder(const MD5Builder&) = delete;
  MD5Builder& operator=(const MD5Builder&) = delete;

  void begin() {
    if (EVP_DigestInit_ex(context, EVP_md5(), nullptr) != 1) throw std::runtime_error("EVP_DigestInit_ex failed");
  }
  void add(const char* text) { add(reinterpret_cast<const uint8_t*>(text), std::strlen(text)); }
  void add(const uint8_t* bytes, const size_t count) {
    auto& fixture = documentIdFixture::state;
    fixture.hashFeedSizes.push_back(count);
    // A baseline negative HalFile::read result wraps to SIZE_MAX. Record that
    // unsafe request without actually reading outside its 1024-byte buffer.
    if (count > 1024) {
      fixture.oversizedHashFeed = true;
      return;
    }
    if (EVP_DigestUpdate(context, bytes, count) != 1) throw std::runtime_error("EVP_DigestUpdate failed");
  }
  void calculate() {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context, digest, &length) != 1 || length != 16) {
      throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    static constexpr char HEX[] = "0123456789abcdef";
    value.clear();
    for (unsigned int i = 0; i < length; ++i) {
      value.push_back(HEX[digest[i] >> 4]);
      value.push_back(HEX[digest[i] & 15]);
    }
  }
  const std::string& toString() const { return value; }

 private:
  EVP_MD_CTX* context;
  std::string value;
};
