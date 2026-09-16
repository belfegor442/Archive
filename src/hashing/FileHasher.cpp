#include "FileHasher.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <openssl/evp.h>
#include <iomanip>
#include <memory>

namespace archive::hashing {

static constexpr size_t BUFFER_SIZE = 8192;

struct EvpCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const { if (ctx) EVP_MD_CTX_free(ctx); }
};
using EvpCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpCtxDeleter>;

static std::string digest_to_hex(const unsigned char* hash, unsigned int hash_len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string FileHasher::hash_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for hashing: " + path);
    }

    EvpCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) throw std::runtime_error("Failed to create EVP context");

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("EVP_DigestInit_ex failed");

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx.get(), buffer, static_cast<size_t>(file.gcount())) != 1)
            throw std::runtime_error("EVP_DigestUpdate failed for: " + path);
        if (file.eof()) break;
    }

    if (file.bad())
        throw std::runtime_error("I/O error reading file: " + path);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash, &hash_len) != 1)
        throw std::runtime_error("EVP_DigestFinal_ex failed for: " + path);

    return digest_to_hex(hash, hash_len);
}

std::string FileHasher::hash_buffer(const void* data, size_t size) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EvpCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) throw std::runtime_error("Failed to create EVP context");

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("EVP_DigestInit_ex failed");
    if (EVP_DigestUpdate(ctx.get(), data, size) != 1)
        throw std::runtime_error("EVP_DigestUpdate failed");
    if (EVP_DigestFinal_ex(ctx.get(), hash, &hash_len) != 1)
        throw std::runtime_error("EVP_DigestFinal_ex failed");

    return digest_to_hex(hash, hash_len);
}

std::string FileHasher::hash_folder(const std::string& path) {
    std::vector<std::string> file_paths;

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
        if (entry.is_regular_file()) {
            file_paths.push_back(entry.path().string());
        }
    }
    if (ec) {
        throw std::runtime_error("Error iterating folder for hashing: " + ec.message());
    }

    std::sort(file_paths.begin(), file_paths.end());

    EvpCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) throw std::runtime_error("Failed to create EVP context");

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("EVP_DigestInit_ex failed for folder hash");

    for (const auto& fp : file_paths) {
        std::string relative = std::filesystem::relative(fp, path).string();
        EVP_DigestUpdate(ctx.get(), relative.c_str(), relative.size());

        std::ifstream file(fp, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Cannot open file for folder hashing: " + fp);

        char buffer[BUFFER_SIZE];
        while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
            if (EVP_DigestUpdate(ctx.get(), buffer, static_cast<size_t>(file.gcount())) != 1)
                throw std::runtime_error("EVP_DigestUpdate failed during folder hashing: " + fp);
            if (file.eof()) break;
        }

        if (file.bad())
            throw std::runtime_error("I/O error during folder hashing: " + fp);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash, &hash_len) != 1)
        throw std::runtime_error("EVP_DigestFinal_ex failed for folder hash");

    return digest_to_hex(hash, hash_len);
}

bool FileHasher::compare(const std::string& h1, const std::string& h2) {
    if (h1.size() != h2.size()) return false;
    unsigned char result = 0;
    for (size_t i = 0; i < h1.size(); i++) {
        result |= static_cast<unsigned char>(h1[i] ^ h2[i]);
    }
    return result == 0;
}

} // namespace archive::hashing
