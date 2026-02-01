#include <io/binary_io.h>

#include <cstdint>
#include <ios>
#include <stdexcept>

namespace Columnar::IO {

BinaryWriter::BinaryWriter(const std::string& filename)
    : file_(filename, std::ios::binary) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
}

BinaryWriter::~BinaryWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

void BinaryWriter::Write(const void* data, size_t size) {
    file_.write(reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
}

void BinaryWriter::WriteString(const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    Write(&length, sizeof(length));

    if (!str.empty()) {
        Write(str.data(), str.size());
    }
}

size_t BinaryWriter::GetPosition() {
    return static_cast<size_t>(file_.tellp());
}

void BinaryWriter::Seek(size_t pos) {
    file_.seekp(static_cast<std::streamoff>(pos), std::ios::beg);
}

void BinaryWriter::Flush() {
    file_.flush();
}

BinaryReader::BinaryReader(const std::string& filename)
    : file_(filename, std::ios::binary) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open file for reading: " + filename);
    }

    file_.seekg(0, std::ios::end);
    fileSize_ = static_cast<size_t>(file_.tellg());
    file_.seekg(0, std::ios::beg);
}

BinaryReader::~BinaryReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

void BinaryReader::Read(void* buffer, size_t size) {
    file_.read(reinterpret_cast<char*>(buffer),
               static_cast<std::streamsize>(size));
}

std::string BinaryReader::ReadString() {
    uint32_t length;
    Read(&length, sizeof(length));
    if (length == 0) {
        return {};
    }

    std::string result(length, '\0');
    Read(result.data(), length);
    return result;
}

size_t BinaryReader::GetPosition() {
    return static_cast<size_t>(file_.tellg());
}

void BinaryReader::Seek(size_t pos) {
    file_.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
}

size_t BinaryReader::GetFileSize() const {
    return fileSize_;
}

}  // namespace Columnar::IO