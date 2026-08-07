#pragma once

// SHA-256 (FIPS 180-4). Split out so it can be unit-tested against the
// published vectors: everything here is bytes -> bytes with no CEF, no
// platform and no filesystem dependency.
//
// Vendored rather than reached for through a platform API. BCrypt exists on
// Windows and CommonCrypto on macOS, but that is two code paths for one
// function, neither of them testable in core/tests, and boot's BCrypt usage
// lives in a different binary. ~150 lines is cheaper than the split.
//
// This is used to name a plugin's storage database, where collision resistance
// against *chosen* input is the whole requirement — see
// docs/plugin-storage.md section 5.2 for why FNV-1a, which `disabled_plugins`
// uses, is not acceptable there.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace sha256
{
    namespace detail
    {
        inline constexpr uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
        };

        inline constexpr uint32_t rotr(uint32_t x, int n)
        {
            return (x >> n) | (x << (32 - n));
        }

        inline constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (~x & z); }
        inline constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }

        inline constexpr uint32_t big_sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
        inline constexpr uint32_t big_sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
        inline constexpr uint32_t small_sigma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
        inline constexpr uint32_t small_sigma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
    }

    constexpr size_t DIGEST_BYTES = 32;

    /// Streaming hasher. `update` any number of times, then `finish` once.
    struct Hasher
    {
        void update(const void *data, size_t length)
        {
            const auto *p = static_cast<const uint8_t *>(data);
            bits_ += static_cast<uint64_t>(length) * 8;

            while (length > 0)
            {
                const size_t room = 64 - buffered_;
                const size_t take = length < room ? length : room;

                std::memcpy(block_ + buffered_, p, take);
                buffered_ += take;
                p += take;
                length -= take;

                if (buffered_ == 64)
                {
                    compress(block_);
                    buffered_ = 0;
                }
            }
        }

        void finish(uint8_t out[DIGEST_BYTES])
        {
            // 0x80, then zeros, then the message length in bits as a 64-bit
            // big-endian trailer, sized so the trailer lands flush at the end
            // of a block.
            //
            // buffered + 1 + zeros + 8 is 64 when buffered < 56 and 128
            // otherwise, so the total is always a whole number of blocks. 72 is
            // the largest that sum's padding part can be (1 + 63 + 8).
            const uint64_t bits = bits_;
            const size_t used = buffered_;
            const size_t zeros = (used < 56) ? (56 - used - 1) : (120 - used - 1);

            uint8_t pad[72] = { 0x80 };
            for (int i = 0; i < 8; ++i)
                pad[1 + zeros + i] = static_cast<uint8_t>(bits >> (56 - 8 * i));

            update(pad, 1 + zeros + 8);

            for (int i = 0; i < 8; ++i)
            {
                out[i * 4 + 0] = static_cast<uint8_t>(h_[i] >> 24);
                out[i * 4 + 1] = static_cast<uint8_t>(h_[i] >> 16);
                out[i * 4 + 2] = static_cast<uint8_t>(h_[i] >> 8);
                out[i * 4 + 3] = static_cast<uint8_t>(h_[i]);
            }
        }

    private:
        void compress(const uint8_t block[64])
        {
            using namespace detail;

            uint32_t w[64];
            for (int t = 0; t < 16; ++t)
            {
                w[t] = (static_cast<uint32_t>(block[t * 4 + 0]) << 24)
                     | (static_cast<uint32_t>(block[t * 4 + 1]) << 16)
                     | (static_cast<uint32_t>(block[t * 4 + 2]) << 8)
                     |  static_cast<uint32_t>(block[t * 4 + 3]);
            }
            for (int t = 16; t < 64; ++t)
                w[t] = small_sigma1(w[t - 2]) + w[t - 7] + small_sigma0(w[t - 15]) + w[t - 16];

            uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
            uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];

            for (int t = 0; t < 64; ++t)
            {
                const uint32_t t1 = h + big_sigma1(e) + ch(e, f, g) + K[t] + w[t];
                const uint32_t t2 = big_sigma0(a) + maj(a, b, c);

                h = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }

            h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
            h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
        }

        uint32_t h_[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
        };
        uint8_t  block_[64] = {};
        size_t   buffered_ = 0;
        uint64_t bits_ = 0;
    };

    /// One-shot digest.
    inline void hash(const void *data, size_t length, uint8_t out[DIGEST_BYTES])
    {
        Hasher hasher;
        hasher.update(data, length);
        hasher.finish(out);
    }

    /// Lowercase hex of the first `bytes` of the digest. `bytes` defaults to
    /// the whole thing; the storage filename uses 16 (128 bits).
    inline std::string hex(const void *data, size_t length, size_t bytes = DIGEST_BYTES)
    {
        if (bytes > DIGEST_BYTES)
            bytes = DIGEST_BYTES;

        uint8_t digest[DIGEST_BYTES];
        hash(data, length, digest);

        static const char *digits = "0123456789abcdef";
        std::string out;
        out.resize(bytes * 2);

        for (size_t i = 0; i < bytes; ++i)
        {
            out[i * 2 + 0] = digits[digest[i] >> 4];
            out[i * 2 + 1] = digits[digest[i] & 0x0F];
        }
        return out;
    }
}
