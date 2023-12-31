#include <vector>
#include <memory.h>
#include "openssl/x509.h"
#include "openssl/pem.h"
#include "openssl/pkcs12.h"
#include "elfSignatureApi.h"
#include "defer.h"

// 读取X509证书的公钥
int GetPublicKeyFromCertificate(const char* filePath, unsigned char* out){
  EVP_PKEY* publicKey = nullptr;
  X509* cert = nullptr;
  std::vector<unsigned char> publicKeyBytes;
  size_t length;
  FILE* file = fopen(filePath, "rb");
  if (file) {
    cert = PEM_read_X509(file, nullptr, nullptr, nullptr);
    fclose(file);
  }

  if (cert) {
    publicKey = X509_get_pubkey(cert);
    X509_free(cert);
  }

  EC_KEY* ecKey = EVP_PKEY_get1_EC_KEY(publicKey);
  if (ecKey) {
    const EC_POINT* ecPoint = EC_KEY_get0_public_key(ecKey);
    if (ecPoint) {
      const EC_GROUP* ecGroup = EC_KEY_get0_group(ecKey);
      if (ecGroup) {
        length = EC_POINT_point2oct(ecGroup, ecPoint, POINT_CONVERSION_UNCOMPRESSED, nullptr, 0, nullptr);
        if (length > 0) {
          publicKeyBytes.resize(length + 1);
          unsigned char* buffer = publicKeyBytes.data();
          if (EC_POINT_point2oct(ecGroup, ecPoint, POINT_CONVERSION_UNCOMPRESSED, buffer, length, nullptr) == length) {
            buffer[0] = 0x04; // 设置开头的0x04标志
          } else {
            publicKeyBytes.clear();
          }
        }
      }
    }
    EC_KEY_free(ecKey);
  }

  memcpy(out, publicKeyBytes.data(), length);

  return 1;
}

int GetPriKeyFromP12File(const char* filePath, const char* password, unsigned char* out, unsigned int* outLen){
  FILE* p12_file_ptr;
  EVP_PKEY* evpPkey = NULL;
  EC_KEY* ecKey = NULL;
  const BIGNUM* priNUM;
  int ret = 0;

  if(filePath == NULL || password == NULL || outLen == NULL){
    return 0;
  }

  p12_file_ptr = fopen(filePath, "rb");
  defer(fclose(p12_file_ptr));
  if(!p12_file_ptr){
    return 2;
  }

  PKCS12* p12 = d2i_PKCS12_fp(p12_file_ptr, NULL);
  defer(PKCS12_free(p12));
  ret = PKCS12_parse(p12, password, &evpPkey, NULL, NULL);
  defer(EVP_PKEY_free(evpPkey));
  if (!ret){
    return 3;
  }

  ecKey = EVP_PKEY_get0_EC_KEY(evpPkey);
  if(ecKey == NULL){
    return 0;
  }

  priNUM = EC_KEY_get0_private_key(ecKey);
  if(priNUM == NULL){
    return 0;
  }

  *outLen = BN_bn2bin(priNUM, out);

  return 1;
}