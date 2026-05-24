#include "file_writer.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <stdexcept>

namespace Columnar::IO {

FileWriter::FileWriter(const std::string& path) {
    constexpr mode_t kDefaultFileMode = 0644;
    fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, kDefaultFileMode);
    if (fd_ < 0) {
        throw std::runtime_error("FileWriter: cannot open: " + path);
    }
}

FileWriter::~FileWriter() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

FileWriter::FileWriter(FileWriter&& other) noexcept
    : fd_(other.fd_),
      pos_(other.pos_) {
    other.fd_ = -1;
    other.pos_ = 0;
}

FileWriter& FileWriter::operator=(FileWriter&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = other.fd_;
        pos_ = other.pos_;
        other.fd_ = -1;
        other.pos_ = 0;
    }
    return *this;
}

void FileWriter::Write(const void* data, size_t n) {
    const ssize_t bytesWritten = ::pwrite(fd_, data, n, static_cast<off_t>(pos_));
    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != n) {
        throw std::runtime_error("FileWriter::Write pwrite failed");
    }
    pos_ += n;
}

void FileWriter::WriteString(const std::string& str) {
    const uint32_t len = static_cast<uint32_t>(str.size());
    Write(&len, sizeof(len));
    if (!str.empty()) {
        Write(str.data(), str.size());
    }
}

void FileWriter::Flush() {
    fsync(fd_);
}

}  // namespace Columnar::IO
