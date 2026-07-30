#include "auto_cattery/save_safety/file_hash.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace autocattery::save_safety {
namespace {

class Sha256Context final {
public:
    Sha256Context() {
        if (BCryptOpenAlgorithmProvider(
                &algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            return;
        }
        DWORD object_size{};
        DWORD result_size{};
        if (BCryptGetProperty(
                algorithm_, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                &result_size, 0) < 0) {
            return;
        }
        object_.resize(object_size);
        BCryptCreateHash(
            algorithm_, &hash_, object_.data(), object_size, nullptr, 0, 0);
    }

    ~Sha256Context() {
        if (hash_ != nullptr) {
            BCryptDestroyHash(hash_);
        }
        if (algorithm_ != nullptr) {
            BCryptCloseAlgorithmProvider(algorithm_, 0);
        }
    }

    [[nodiscard]] bool Update(const void* bytes, std::size_t size) const {
        return hash_ != nullptr &&
            (size == 0 || BCryptHashData(
                hash_, const_cast<PUCHAR>(
                    static_cast<const UCHAR*>(bytes)),
                static_cast<ULONG>(size), 0) >= 0);
    }

    [[nodiscard]] Result<std::string> Finish() const {
        std::array<unsigned char, 32> digest{};
        if (hash_ == nullptr || BCryptFinishHash(
                hash_, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
            return {{}, ErrorCode::BackupFailed, "SHA-256 calculation failed"};
        }
        std::ostringstream encoded;
        encoded << std::hex << std::setfill('0');
        for (const auto byte : digest) {
            encoded << std::setw(2) << static_cast<unsigned int>(byte);
        }
        return {encoded.str()};
    }

private:
    BCRYPT_ALG_HANDLE algorithm_{};
    BCRYPT_HASH_HANDLE hash_{};
    std::vector<unsigned char> object_;
};

}  // namespace

Result<std::string> Sha256File(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {{}, ErrorCode::BackupFailed, "file could not be opened for hashing"};
    }
    Sha256Context context;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && !context.Update(buffer.data(),
                static_cast<std::size_t>(count))) {
            return {{}, ErrorCode::BackupFailed, "SHA-256 update failed"};
        }
    }
    if (!input.eof()) {
        return {{}, ErrorCode::BackupFailed, "file read failed while hashing"};
    }
    return context.Finish();
}

Result<std::string> Sha256Text(std::string_view text) {
    Sha256Context context;
    if (!context.Update(text.data(), text.size())) {
        return {{}, ErrorCode::BackupFailed, "SHA-256 update failed"};
    }
    return context.Finish();
}

}  // namespace autocattery::save_safety
