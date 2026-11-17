#include "SnapshotStore.hpp"
#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <type_traits>
#include <unistd.h>

namespace shop {
namespace {
std::string ioError(std::string_view operation) {
    return std::string(operation) + ": " + std::strerror(errno);
}
class PendingFile {
  public:
    explicit PendingFile(const std::filesystem::path& destination) {
        static std::atomic<std::uint64_t> sequence{0};
        const auto time = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = destination;
            path_ += ".tmp-" + std::to_string(::getpid()) + "-" + std::to_string(time) + "-" +
                     std::to_string(sequence.fetch_add(1));
            descriptor_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (descriptor_ >= 0) {
                return;
            }
            if (errno != EEXIST) {
                throw SnapshotException(ioError("Не удалось создать временный снимок"));
            }
        }
        throw SnapshotException("Не удалось выбрать уникальное имя временного снимка");
    }
    PendingFile(const PendingFile&) = delete;
    PendingFile& operator=(const PendingFile&) = delete;
    ~PendingFile() {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }
        if (!committed_) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }
    void write(std::string_view document) {
        std::size_t offset = 0;
        while (offset < document.size()) {
            const auto count =
                ::write(descriptor_, document.data() + offset, document.size() - offset);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                throw SnapshotException(ioError("Ошибка записи временного снимка"));
            }
            offset += static_cast<std::size_t>(count);
        }
        const auto descriptor = descriptor_;
        descriptor_ = -1;
        if (::close(descriptor) != 0) {
            throw SnapshotException(ioError("Ошибка закрытия временного снимка"));
        }
    }
    void commit(const std::filesystem::path& destination) {
        std::error_code error;
        std::filesystem::rename(path_, destination, error);
        if (error) {
            throw SnapshotException("Не удалось заменить снимок: " + error.message());
        }
        committed_ = true;
    }

  private:
    std::filesystem::path path_;
    int descriptor_ = -1;
    bool committed_ = false;
};
std::string readDocument(const std::filesystem::path& source) {
    std::ifstream input(source, std::ios::binary | std::ios::ate);
    if (!input) {
        throw SnapshotException("Не удалось открыть файл снимка");
    }
    const auto size = input.tellg();
    snapshot::require(size >= 0 &&
                          size <= static_cast<std::streamoff>(snapshot::MaximumSnapshotBytes),
                      "Недопустимый размер файла снимка");
    std::string result(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    input.read(result.data(), static_cast<std::streamsize>(result.size()));
    snapshot::require(input.gcount() == static_cast<std::streamsize>(result.size()) && !input.bad(),
                      "Файл снимка прочитан не полностью");
    const auto extra = input.get();
    snapshot::require(extra == std::char_traits<char>::eof() && input.eof(),
                      "Размер файла изменился во время чтения");
    return result;
}
} // namespace
void SnapshotStore::save(const std::filesystem::path& destination, const StoreState& state) {
    snapshot::require(!destination.empty() && !destination.filename().empty(),
                      "Не задано имя файла снимка");
    const auto document = SnapshotCodec::encode(state);
    PendingFile temporary(destination);
    temporary.write(document);
    temporary.commit(destination);
}
void SnapshotStore::load(const std::filesystem::path& source, StoreState& state) {
    auto candidate = SnapshotCodec::decode(readDocument(source));
    static_assert(std::is_nothrow_swappable_v<StoreState>);
    using std::swap;
    swap(state, candidate);
}
} // namespace shop
