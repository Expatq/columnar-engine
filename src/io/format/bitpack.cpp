#include <io/format/bitpack.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include <cstring>
#include <vector>

namespace Columnar::IO {

void BitpackEncode(const uint32_t* values, size_t n, uint8_t bitWidth, uint8_t* dst) {
    uint64_t buf = 0;
    int used = 0;
    size_t bi = 0;

    for (size_t i = 0; i < n; ++i) {
        buf |= static_cast<uint64_t>(values[i]) << used;
        used += bitWidth;
        while (used >= 8) {
            dst[bi++] = static_cast<uint8_t>(buf);
            buf >>= 8;
            used -= 8;
        }
    }

    if (used > 0)
        dst[bi] = static_cast<uint8_t>(buf);
}

static void DecodeScalarI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                            int64_t minVal, int32_t* dst) {
    const uint32_t mask = (bitWidth == 32) ? ~0u : ((1u << bitWidth) - 1u);
    uint64_t buf = 0;
    int bits = 0;
    size_t bi = 0;

    for (size_t i = 0; i < n; ++i) {
        while (bits < bitWidth) {
            buf |= static_cast<uint64_t>(src[bi++]) << bits;
            bits += 8;
        }
        dst[i] = static_cast<int32_t>(static_cast<int64_t>(buf & mask) + minVal);
        buf >>= bitWidth;
        bits -= bitWidth;
    }
}

#ifdef __AVX2__

static void DecodeAvx8(const uint8_t* src, size_t n, int64_t minVal, int32_t* dst) {
    const __m256i vMin = _mm256_set1_epi32(static_cast<int32_t>(minVal));
    size_t i = 0;

    for (; i + 8 <= n; i += 8, src += 8) {
        __m128i raw = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src));
        __m256i vals = _mm256_cvtepu8_epi32(raw);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i),
                            _mm256_add_epi32(vals, vMin));
    }

    for (; i < n; ++i)
        dst[i] = static_cast<int32_t>(static_cast<int64_t>(src[i]) + minVal);
}

static void DecodeAvx16(const uint8_t* src, size_t n, int64_t minVal, int32_t* dst) {
    const __m256i vMin = _mm256_set1_epi32(static_cast<int32_t>(minVal));
    size_t i = 0;

    for (; i + 8 <= n; i += 8, src += 16) {
        __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        __m256i vals = _mm256_cvtepu16_epi32(raw);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i),
                            _mm256_add_epi32(vals, vMin));
    }

    for (; i < n; ++i, src += 2) {
        uint16_t v;
        std::memcpy(&v, src, 2);
        dst[i] = static_cast<int32_t>(static_cast<int64_t>(v) + minVal);
    }
}

#endif

void BitpackDecodeI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int32_t* dst) {
#ifdef __AVX2__
    if (bitWidth == 8) {
        DecodeAvx8(src, n, minVal, dst);
        return;
    }
    if (bitWidth == 16) {
        DecodeAvx16(src, n, minVal, dst);
        return;
    }
#endif
    DecodeScalarI32(src, n, bitWidth, minVal, dst);
}

void BitpackDecodeI16(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int16_t* dst) {
    std::vector<int32_t> tmp(n);
    BitpackDecodeI32(src, n, bitWidth, minVal, tmp.data());
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<int16_t>(tmp[i]);
}

}  // namespace Columnar::IO
