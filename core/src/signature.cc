// The `.pengu` section: where build/sign-core.ps1 writes the signature that
// boot.dll checks before loading this core into the client.
//
// Format and signed region are specified in boot/trust.h. Nothing in core ever
// reads this — the boot reads it out of the file on disk, before the DLL is
// mapped — so the only job here is to reserve the bytes at a location both
// halves can find, which is the raw-data start of a section named `.pengu`.
//
// See bootstrap.h for why this guards on _WIN32 rather than pengu.h's OS_WIN:
// the macOS makefile globs src/*.cc, and there is no Authenticode or IFEO on
// that side to have a signature for.
#ifdef _WIN32

#include <cstdint>

namespace
{
    struct Signature
    {
        uint8_t  magic[8];      // "PENGUSIG"
        uint32_t format;        // 1
        uint32_t flags;
        uint8_t  signature[64]; // raw ECDSA P-256 r||s, filled in after build
        uint8_t  reserved[432];
    };

    static_assert(sizeof(Signature) == 512, "boot.dll reads a fixed 512-byte blob");
}

#pragma section(".pengu", read)

// Zeroed at compile time, which the boot reports as "never signed" rather than
// as a bad signature — the distinction matters, because unsigned builds are a
// normal output of every CI run that didn't opt into code signing.
extern "C" __declspec(allocate(".pengu")) const Signature pengu_signature = {
    { 'P', 'E', 'N', 'G', 'U', 'S', 'I', 'G' },
    1,
    0,
    {},
    {},
};

// No code references it, so without this the linker is free to drop the whole
// section and the signing step would have nowhere to write.
#pragma comment(linker, "/INCLUDE:pengu_signature")

#endif // _WIN32
