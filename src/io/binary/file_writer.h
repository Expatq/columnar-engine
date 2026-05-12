#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Columnar::IO {

class FileWriter {
public:
    explicit FileWriter(const std::string& path);
    ~FileWriter();

    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&&) noexcept;
    FileWriter& operator=(FileWriter&&) noexcept;

    void Write(const void* data, size_t n);

    void WriteString(const std::string& str);

    size_t GetPosition() const {
        return pos_;
    }

    void Seek(size_t pos) {
        pos_ = pos;
    }

    void Flush();

private:
    int fd_ = -1;
    size_t pos_ = 0;
};

}  // namespace Columnar::IO
