#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Columnar::IO {

constexpr uint8_t kMagicBytes[4] = {'I', 'Y', 'X', 0x01};
constexpr size_t kMagicSize = 4;
constexpr size_t kHeaderSize = 64;

class BinaryWriter {
public:
    explicit BinaryWriter(const std::string& filename);

    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;

    BinaryWriter(BinaryWriter&&) = default;
    BinaryWriter& operator=(BinaryWriter&&) = default;

    void Write(const void* data, size_t size);
    void WriteString(const std::string& str);  // length-prefixed (uint32)

    size_t GetPosition();
    void Seek(size_t position);
    void Flush();

    ~BinaryWriter();

private:
    std::ofstream file_;
};

class BinaryReader {
public:
    explicit BinaryReader(const std::string& filename);

    BinaryReader(const BinaryReader&) = delete;
    BinaryReader& operator=(const BinaryReader&) = delete;

    BinaryReader(BinaryReader&&) = default;
    BinaryReader& operator=(BinaryReader&&) = default;

    void Read(void* buffer, size_t size);
    std::string ReadString();  // length-prefixed (uint32)

    size_t GetPosition();
    void Seek(size_t position);
    size_t GetFileSize() const;

    ~BinaryReader();

private:
    std::ifstream file_;
    size_t fileSize_ = 0;
};

}  // namespace Columnar::IO