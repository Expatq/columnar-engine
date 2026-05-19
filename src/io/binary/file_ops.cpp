#include "file_ops.h"

#include <util/size_literals.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#endif

namespace Columnar::IO {

namespace {

std::runtime_error SysError(const char* where) {
    return std::runtime_error(std::string(where) + ": " + std::strerror(errno));
}

}  // namespace

void ReadAll(int fd, void* dst, size_t n, uint64_t offset) {
    auto* out = static_cast<char*>(dst);
    size_t done = 0;

    while (done < n) {
        const ssize_t got = pread(fd, out + done, n - done, static_cast<off_t>(offset + done));
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw SysError("pread");
        }
        if (got == 0) {
            throw std::runtime_error("pread: unexpected EOF");
        }
        done += static_cast<size_t>(got);
    }
}

void WriteAll(int fd, const void* src, size_t n, uint64_t offset) {
    const auto* in = static_cast<const char*>(src);
    size_t done = 0;

    while (done < n) {
        const ssize_t wrote = ::pwrite(fd, in + done, n - done, static_cast<off_t>(offset + done));
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw SysError("pwrite");
        }
        if (wrote == 0) {
            throw std::runtime_error("pwrite: wrote zero bytes");
        }
        done += static_cast<size_t>(wrote);
    }
}

uint64_t GetFileSize(int fd) {
    struct stat st{};
    if (::fstat(fd, &st) < 0) {
        throw SysError("fstat");
    }
    return static_cast<uint64_t>(st.st_size);
}

void CopyFileRange(int srcFd, uint64_t srcOffset, int dstFd, uint64_t dstOffset, uint64_t n) {
#if defined(__linux__)
    off_t inOff = static_cast<off_t>(srcOffset);
    off_t outOff = static_cast<off_t>(dstOffset);
    uint64_t copied = 0;

    while (copied < n) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(n - copied, 128_MB));
        const ssize_t ret = ::copy_file_range(srcFd, &inOff, dstFd, &outOff, chunk, 0);
        if (ret > 0) {
            copied += static_cast<uint64_t>(ret);
            continue;
        }
        if (ret == 0) {
            throw std::runtime_error("copy_file_range: unexpected EOF");
        }
        if (errno != EXDEV && errno != EINVAL && errno != ENOSYS && errno != EPERM) {
            if (errno == EINTR) {
                continue;
            }
            throw SysError("copy_file_range");
        }
        break;
    }
    if (copied == n) {
        return;
    }
    srcOffset += copied;
    dstOffset += copied;
    n -= copied;
#endif

    constexpr size_t kBufferSize = 64_MB;
    std::vector<char> buffer(kBufferSize);
    uint64_t done = 0;

    while (done < n) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(n - done, buffer.size()));
        ReadAll(srcFd, buffer.data(), chunk, srcOffset + done);
        WriteAll(dstFd, buffer.data(), chunk, dstOffset + done);
        done += chunk;
    }
}

}  // namespace Columnar::IO
