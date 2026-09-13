#include "FileHasher.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <openssl/evp.h>
#include <iomanip>

namespace archive::hashing {

static const size_t BUFFER_SIZE = 8192;

std::string FileHasher::hash_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for hashing: " + path);
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create EVP context");

    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buffer, static_cast<size_t>(file.gcount()));
        if (file.eof()) break;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string FileHasher::hash_buffer(const void* data, size_t size) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create EVP context");

    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, size);
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

} // namespace archive::hashing
