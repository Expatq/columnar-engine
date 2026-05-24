#include "mmap_file.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace Columnar::IO {

MmapFile::MmapFile(const std::string& path) {
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0)
        throw std::runtime_error("MmapFile: cannot open: " + path);

    struct stat st{};
    if (fstat(fd_, &st) < 0) {
        close(fd_);
        throw std::runtime_error("MmapFile: fstat failed: " + path);
    }
    size_ = static_cast<size_t>(st.st_size);
    if (size_ == 0) {
        close(fd_);
        throw std::runtime_error("MmapFile: empty file: " + path);
    }

    void* p = mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd_, 0);
    if (p == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("MmapFile: mmap failed: " + path);
    }
    data_ = static_cast<const uint8_t*>(p);
}

MmapFile::~MmapFile() {
    if (data_) munmap(const_cast<uint8_t*>(data_), size_);
    if (fd_ >= 0) close(fd_);
}

MmapFile::MmapFile(MmapFile&& other) noexcept
    : data_(other.data_), size_(other.size_), fd_(other.fd_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_   = -1;
}

MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
    if (this != &other) {
        if (data_) munmap(const_cast<uint8_t*>(data_), size_);
        if (fd_ >= 0) close(fd_);
        data_ = other.data_;
        size_ = other.size_;
        fd_   = other.fd_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_   = -1;
    }
    return *this;
}

void MmapFile::Read(size_t offset, void* dst, size_t n) const {
    if (offset + n > size_)
        throw std::out_of_range("MmapFile::Read: out of range");
    std::memcpy(dst, data_ + offset, n);
}

const uint8_t* MmapFile::Ptr(size_t offset) const {
    if (offset > size_)
        throw std::out_of_range("MmapFile::Ptr: out of range");
    return data_ + offset;
}

void MmapFile::Prefetch(size_t offset, size_t n) const {
    if (offset >= size_) return;
    if (offset + n > size_) n = size_ - offset;
    madvise(const_cast<uint8_t*>(data_) + offset, n, MADV_WILLNEED);
}

void MmapFile::HintSequential(size_t offset, size_t n) const {
    if (offset >= size_) return;
    if (offset + n > size_) n = size_ - offset;
    madvise(const_cast<uint8_t*>(data_) + offset, n, MADV_SEQUENTIAL);
}

}  // namespace Columnar::IO
