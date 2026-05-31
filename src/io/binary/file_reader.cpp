#include "file_reader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdexcept>

namespace Columnar::IO {

FileReader::FileReader(const std::string& path) {
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("FileReader: cannot open: " + path);
    }

    struct stat st{};
    if (fstat(fd_, &st) < 0) {
        close(fd_);
        throw std::runtime_error("FileReader: fstat failed: " + path);
    }

    size_ = static_cast<size_t>(st.st_size);
    if (size_ == 0) {
        close(fd_);
        throw std::runtime_error("FileReader: empty file: " + path);
    }
#ifdef __linux__
    posix_fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
}

FileReader::~FileReader() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

FileReader::FileReader(FileReader&& other) noexcept
    : fd_(other.fd_),
      size_(other.size_) {
    other.fd_ = -1;
    other.size_ = 0;
}

FileReader& FileReader::operator=(FileReader&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            close(fd_);
        }

        fd_ = other.fd_;
        size_ = other.size_;
        other.fd_ = -1;
        other.size_ = 0;
    }
    return *this;
}

void FileReader::Read(size_t offset, void* dst, size_t n) const {
    if (offset + n > size_) {
        throw std::out_of_range("FileReader::Read out of range");
    }

    const ssize_t bytesRead = pread(fd_, dst, n, static_cast<off_t>(offset));
    if (bytesRead < 0 || static_cast<size_t>(bytesRead) != n) {
        throw std::runtime_error("FileReader::Read pread failed");
    }
}

void FileReader::Prefetch(size_t offset, size_t n) const {
    if (offset >= size_) {
        return;
    }
    if (offset + n > size_) {
        n = size_ - offset;
    }
#ifdef __linux__
    posix_fadvise(fd_, static_cast<off_t>(offset), static_cast<off_t>(n), POSIX_FADV_WILLNEED);
#endif
}

}  // namespace Columnar::IO
