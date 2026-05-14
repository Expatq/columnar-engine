#pragma once

#include <cstddef>
#include <string>

namespace Columnar::IO {

class FileReader {
public:
    explicit FileReader(const std::string& path);
    ~FileReader();

    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    FileReader(FileReader&&) noexcept;
    FileReader& operator=(FileReader&&) noexcept;

    void Read(size_t offset, void* dst, size_t n) const;
    size_t GetFileSize() const { return size_; }

    // posix_fadvise(WILLNEED) — асинхронный prefetch диапазона в page cache.
    void Prefetch(size_t offset, size_t n) const;

private:
    int    fd_   = -1;
    size_t size_ = 0;
};

}  // namespace Columnar::IO
