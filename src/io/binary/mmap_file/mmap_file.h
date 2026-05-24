#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Columnar::IO {

class MmapFile {
public:
    explicit MmapFile(const std::string& path);
    ~MmapFile();

    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;

    MmapFile(MmapFile&&) noexcept;
    MmapFile& operator=(MmapFile&&) noexcept;

    void Read(size_t offset, void* dst, size_t n) const;
    const uint8_t* Ptr(size_t offset) const;

    size_t GetFileSize() const {
        return size_;
    }

    void Prefetch(size_t offset, size_t n) const;
    void HintSequential(size_t offset, size_t n) const;

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    int fd_ = -1;
};

}  // namespace Columnar::IO
