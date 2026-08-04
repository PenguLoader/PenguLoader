#include "trust.h"
#include "trust_key.h"

#include <bcrypt.h>
#include <softpub.h>
#include <wintrust.h>

#include <cstring>
#include <vector>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// The blob format and the signed region are specified in trust.h. Keep this
// file and build/sign-core.ps1 in step — they are the two halves of one
// definition, and a disagreement between them shows up as a release that
// silently refuses to activate.

namespace
{
    namespace fs = std::filesystem;

    constexpr uint8_t SECTION_NAME[8] = { '.', 'p', 'e', 'n', 'g', 'u', 0, 0 };
    constexpr uint8_t BLOB_MAGIC[8]   = { 'P', 'E', 'N', 'G', 'U', 'S', 'I', 'G' };

    constexpr size_t BLOB_SIZE   = 512;
    constexpr size_t BLOB_FORMAT = 8;    // u32, within the blob
    constexpr size_t BLOB_SIG    = 16;   // the signature field, within the blob
    constexpr size_t SIG_SIZE    = 64;

    constexpr uint32_t FORMAT_VERSION = 1;

    // 64 MB. A core an order of magnitude larger than anything we ship is a
    // reason to stop, not to allocate.
    constexpr uint64_t MAX_CORE_BYTES = 64ull * 1024 * 1024;

    /// Where in the file each thing we care about lives. Everything downstream
    /// works from this, so a PE we can't fully pin down is refused rather than
    /// hashed on a guess.
    struct Layout
    {
        size_t blob      = 0;   // start of the 512-byte blob
        size_t signature = 0;   // the 64-byte field inside it
        size_t checksum  = 0;   // OptionalHeader.CheckSum
        size_t security  = 0;   // DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY]
        size_t signedEnd = 0;   // SizeOfHeaders + sum(SizeOfRawData)
    };

    bool read_u16(const std::vector<uint8_t> &f, size_t at, uint16_t &out)
    {
        if (at + 2 > f.size())
            return false;
        out = static_cast<uint16_t>(f[at] | (f[at + 1] << 8));
        return true;
    }

    bool read_u32(const std::vector<uint8_t> &f, size_t at, uint32_t &out)
    {
        if (at + 4 > f.size())
            return false;
        out = static_cast<uint32_t>(f[at])
            | (static_cast<uint32_t>(f[at + 1]) << 8)
            | (static_cast<uint32_t>(f[at + 2]) << 16)
            | (static_cast<uint32_t>(f[at + 3]) << 24);
        return true;
    }

    /// Read the whole file through the handle we already hold. Reopening by
    /// path would reintroduce the swap window this design exists to close.
    bool read_all(HANDLE file, std::vector<uint8_t> &out)
    {
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(file, zero, NULL, FILE_BEGIN))
            return false;

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0
            || static_cast<uint64_t>(size.QuadPart) > MAX_CORE_BYTES)
            return false;

        out.resize(static_cast<size_t>(size.QuadPart));

        size_t at = 0;
        while (at < out.size())
        {
            size_t remaining = out.size() - at;
            DWORD chunk = static_cast<DWORD>(remaining > 1024 * 1024 ? 1024 * 1024 : remaining);
            DWORD read = 0;
            if (!ReadFile(file, out.data() + at, chunk, &read, NULL) || read == 0)
                return false;
            at += read;
        }

        return true;
    }

    /// Pin down the PE. PE32+ only, which is all we ship and all LCUX loads.
    trust::Result locate(const std::vector<uint8_t> &f, Layout &out)
    {
        uint16_t mz;
        if (!read_u16(f, 0, mz) || mz != 0x5A4D)              // 'MZ'
            return trust::Result::NotPengu;

        uint32_t lfanew;
        if (!read_u32(f, 0x3C, lfanew))
            return trust::Result::NotPengu;

        uint32_t pe;
        if (!read_u32(f, lfanew, pe) || pe != 0x00004550)     // 'PE\0\0'
            return trust::Result::NotPengu;

        size_t fileHeader = static_cast<size_t>(lfanew) + 4;

        uint16_t sectionCount, optionalSize;
        if (!read_u16(f, fileHeader + 2, sectionCount)
            || !read_u16(f, fileHeader + 16, optionalSize))
            return trust::Result::Malformed;

        size_t optional = fileHeader + 20;

        uint16_t magic;
        if (!read_u16(f, optional, magic) || magic != 0x20B)  // PE32+
            return trust::Result::Malformed;

        // The security directory entry has to actually be inside the optional
        // header, and NumberOfRvaAndSizes has to reach index 4, or the hole we
        // punch would land on unrelated bytes.
        uint32_t directoryCount;
        if (optionalSize < 152
            || !read_u32(f, optional + 108, directoryCount) || directoryCount <= 4)
            return trust::Result::Malformed;

        uint32_t sizeOfHeaders;
        if (!read_u32(f, optional + 60, sizeOfHeaders))
            return trust::Result::Malformed;

        out.checksum = optional + 64;
        out.security = optional + 144;

        size_t table = optional + optionalSize;
        uint64_t signedEnd = sizeOfHeaders;
        bool found = false;

        for (uint16_t i = 0; i < sectionCount; ++i)
        {
            size_t header = table + static_cast<size_t>(i) * 40;
            if (header + 40 > f.size())
                return trust::Result::Malformed;

            uint32_t rawSize, rawPointer;
            if (!read_u32(f, header + 16, rawSize) || !read_u32(f, header + 20, rawPointer))
                return trust::Result::Malformed;

            signedEnd += rawSize;

            if (memcmp(&f[header], SECTION_NAME, sizeof(SECTION_NAME)) != 0)
                continue;

            // Two sections claiming the name is not something to pick a
            // winner from.
            if (found || rawSize < BLOB_SIZE
                || static_cast<uint64_t>(rawPointer) + BLOB_SIZE > f.size())
                return trust::Result::Malformed;

            out.blob = rawPointer;
            found = true;
        }

        if (!found)
            return trust::Result::NotPengu;

        if (signedEnd > f.size())
            return trust::Result::Malformed;

        out.signature = out.blob + BLOB_SIG;
        out.signedEnd = static_cast<size_t>(signedEnd);

        // The three holes must sit inside the covered region in ascending
        // order. They always do for a linker-produced PE — the blob lives in a
        // section, so it follows the headers — but the walk below would
        // silently produce a digest nobody signed if they didn't.
        if (out.checksum + 4 > out.security
            || out.security + 8 > out.signature
            || out.signature + SIG_SIZE > out.signedEnd)
            return trust::Result::Malformed;

        return trust::Result::Ok;
    }

    bool blob_is_ours(const std::vector<uint8_t> &f, const Layout &l)
    {
        if (memcmp(&f[l.blob], BLOB_MAGIC, sizeof(BLOB_MAGIC)) != 0)
            return false;

        uint32_t format;
        return read_u32(f, l.blob + BLOB_FORMAT, format) && format == FORMAT_VERSION;
    }

    bool is_empty(const uint8_t *bytes, size_t length)
    {
        uint8_t bits = 0;
        for (size_t i = 0; i < length; ++i)
            bits |= bytes[i];
        return bits == 0;
    }

    /// SHA-256 over `[0, signedEnd)` with the three holes skipped.
    bool digest_of(const std::vector<uint8_t> &f, const Layout &l, uint8_t out[32])
    {
        const struct { size_t at, length; } regions[] = {
            { 0,                      l.checksum },
            { l.checksum + 4,         l.security - (l.checksum + 4) },
            { l.security + 8,         l.signature - (l.security + 8) },
            { l.signature + SIG_SIZE, l.signedEnd - (l.signature + SIG_SIZE) },
        };

        BCRYPT_ALG_HANDLE alg = NULL;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != STATUS_SUCCESS)
            return false;

        bool ok = false;
        BCRYPT_HASH_HANDLE hash = NULL;

        if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == STATUS_SUCCESS)
        {
            ok = true;

            for (const auto &region : regions)
            {
                if (region.length == 0)
                    continue;

                if (BCryptHashData(hash, const_cast<PUCHAR>(f.data() + region.at),
                        static_cast<ULONG>(region.length), 0) != STATUS_SUCCESS)
                {
                    ok = false;
                    break;
                }
            }

            if (ok)
                ok = BCryptFinishHash(hash, out, 32, 0) == STATUS_SUCCESS;

            BCryptDestroyHash(hash);
        }

        BCryptCloseAlgorithmProvider(alg, 0);
        return ok;
    }

    /// Verify a raw 64-byte ECDSA P-256 signature over an already-computed
    /// digest, against one of the public points baked into this boot.
    bool verify_with(const uint8_t publicPoint[64], const uint8_t digest[32], const uint8_t *signature)
    {
        BCRYPT_ALG_HANDLE alg = NULL;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0) != STATUS_SUCCESS)
            return false;

        bool ok = false;

        // BCRYPT_ECCKEY_BLOB header followed by X || Y.
        std::vector<uint8_t> blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64);
        auto *header = reinterpret_cast<BCRYPT_ECCKEY_BLOB *>(blob.data());
        header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
        header->cbKey = 32;
        memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), publicPoint, 64);

        BCRYPT_KEY_HANDLE key = NULL;
        if (BCryptImportKeyPair(alg, NULL, BCRYPT_ECCPUBLIC_BLOB, &key,
                blob.data(), static_cast<ULONG>(blob.size()), 0) == STATUS_SUCCESS)
        {
            ok = BCryptVerifySignature(key, NULL, const_cast<PUCHAR>(digest), 32,
                    const_cast<PUCHAR>(signature), SIG_SIZE, 0) == STATUS_SUCCESS;

            BCryptDestroyKey(key);
        }

        BCryptCloseAlgorithmProvider(alg, 0);
        return ok;
    }

    /// Authenticode, publisher-agnostic. WTD_LIFETIME_SIGNING_FLAG accepts a
    /// signature from an expired certificate only when it was timestamped
    /// inside the validity window — ours always is — and rejects
    /// expired-and-untimestamped outright. Revocation is cache-only because
    /// this check gates the client starting; a flaky network must not add
    /// seconds to every launch.
    bool authenticode_ok(HANDLE file, const fs::path &path)
    {
        auto native = path.wstring();

        WINTRUST_FILE_INFO fileInfo{};
        fileInfo.cbStruct = sizeof(fileInfo);
        fileInfo.pcwszFilePath = native.c_str();
        fileInfo.hFile = file;

        GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;

        WINTRUST_DATA data{};
        data.cbStruct = sizeof(data);
        data.dwUIChoice = WTD_UI_NONE;
        data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
        data.dwUnionChoice = WTD_CHOICE_FILE;
        data.pFile = &fileInfo;
        data.dwStateAction = WTD_STATEACTION_VERIFY;
        data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_LIFETIME_SIGNING_FLAG;

        LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);

        data.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);

        return status == ERROR_SUCCESS;
    }
}

const char *trust::describe(Result r)
{
    switch (r)
    {
    // Phrased to complete "<path>: ...", so they don't repeat the filename
    // the refusal line already carries.
    case Result::Ok:              return "ok";
    case Result::NotPengu:        return "carries no Pengu signature block";
    case Result::Malformed:       return "is not a PE this boot can verify";
    case Result::Unsigned:        return "was never signed, and an unsigned build cannot be activated";
    case Result::BadSignature:    return "is not signed by a key this boot trusts";
    case Result::NotAuthenticode: return "failed Authenticode verification";
    case Result::IoError:         return "could not be read";
    }
    return "unknown";
}

trust::Result trust::verify(HANDLE file, const fs::path &core)
{
    std::vector<uint8_t> image;
    if (!read_all(file, image))
        return Result::IoError;

    Layout layout;
    if (auto result = locate(image, layout); result != Result::Ok)
        return result;

    if (!blob_is_ours(image, layout))
        return Result::NotPengu;

    const uint8_t *signature = image.data() + layout.signature;
    if (is_empty(signature, SIG_SIZE))
        return Result::Unsigned;

    uint8_t digest[32];
    if (!digest_of(image, layout, digest))
        return Result::IoError;

    // The primary key, then the announced successor if there is one. Two slots
    // are what let a key rotation be published a release before it is used;
    // see trust_key.h for why that ordering isn't optional.
    bool trusted = verify_with(PENGU_TRUST_PUBKEY, digest, signature);

    if (!trusted && !is_empty(PENGU_TRUST_PUBKEY_NEXT, sizeof(PENGU_TRUST_PUBKEY_NEXT)))
        trusted = verify_with(PENGU_TRUST_PUBKEY_NEXT, digest, signature);

    if (!trusted)
        return Result::BadSignature;

#ifdef _DEBUG
    // Development builds skip Authenticode: locally-built cores are unsigned,
    // and without this nobody can run what they just compiled.
    //
    // Compile-time rather than a runtime bypass file on purpose. A file would
    // have to live somewhere only an administrator can write, which is one
    // more thing to get wrong at install time, and it would exist in Release
    // builds as something to find and abuse. This way the Release boot has no
    // bypass at all. The embedded signature is still enforced above, so a
    // debug boot is not an open door.
    (void)&authenticode_ok;
    (void)core;
#else
    if (!authenticode_ok(file, core))
        return Result::NotAuthenticode;
#endif

    return Result::Ok;
}
