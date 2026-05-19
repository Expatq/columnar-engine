#pragma once

#include <cstddef>
#include <cstdint>

namespace Columnar::IO {

void ReadAll(int fd, void* dst, size_t n, uint64_t offset);
void WriteAll(int fd, const void* src, size_t n, uint64_t offset);
uint64_t GetFileSize(int fd);

/*
    For files of one extension
*/
void CopyFileRange(int srcFd, uint64_t srcOffset, int dstFd, uint64_t dstOffset, uint64_t n);

}  // namespace Columnar::IO
