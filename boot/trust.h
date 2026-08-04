#pragma once

// Trust checks the boot runs before injecting anything.
//
// Two independent gates, both required:
//
//   1. A signature embedded in core.dll itself, made with Pengu's own key (the
//      public half is baked into the boot). This is the authority on *what may
//      load*.
//   2. Authenticode on core.dll — publisher-agnostic, because the embedded
//      signature already fixes the bytes. It exists so that an attacker who
//      somehow got hold of the signing key would ALSO need a code-signing
//      certificate.
//
// Deliberately not gated on the code-signing certificate's identity: pinning
// SignPath's thumbprint would make certificate renewal a boot-replacement
// event, and "signed by SignPath Foundation" is a weak claim anyway — that
// certificate is shared across many unrelated open-source projects.
//
// ---------------------------------------------------------------------------
// The embedded signature
//
// A 512-byte blob in core.dll's own `.pengu` PE section, written by
// build/sign-core.ps1 *before* the file goes for code signing:
//
//     offset  size  field
//     0       8     magic   "PENGUSIG"
//     8       4     format  (u32 LE) — 1
//     12      4     flags   (u32 LE)
//     16      64    sig     raw ECDSA P-256 r||s, all-zero when unsigned
//     80      432   reserved
//
// What the signature covers is the whole file from byte 0 to
// `SizeOfHeaders + sum(SizeOfRawData)`, with three holes punched out:
//
//     OptionalHeader.CheckSum                    4 bytes
//     DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY]   8 bytes
//     the 64-byte `sig` field itself
//
// The first two are exactly what signtool rewrites; the third is unavoidable.
// The upper bound is derived from the headers rather than being the file size
// because everything past the last section's raw data belongs to signtool —
// alignment padding and the appended certificate table — so bounding there
// makes the digest bit-identical before and after code signing without the
// verifier having to reason about whether padding happened. It still covers
// every header and every section, so the entry point, the imports and all code
// are inside it.
//
// This replaces the `pengu.manifest` sidecar, which had to be generated after
// code signing (Authenticode rewrites the PE, so a hash taken before signing
// named bytes that no longer existed). Embedding inverts that ordering and
// removes a file that could be lost, swapped, or forgotten.
// ---------------------------------------------------------------------------

#include <windows.h>
#include <filesystem>

namespace trust
{
    enum class Result
    {
        Ok,
        NotPengu,          // no .pengu section — not one of our cores at all
        Malformed,         // ours, but not a PE we can hash deterministically
        Unsigned,          // the signature field is empty: never signed
        BadSignature,      // present, but not made by a key this boot trusts
        NotAuthenticode,   // failed WinVerifyTrust
        IoError,           // couldn't read core.dll
    };

    /// A short phrase naming what went wrong, written to boot.log and surfaced
    /// in the hub. Reads as a diagnosis, because a refusal is far more likely
    /// to be a signing or rollout mistake than an attack.
    const char *describe(Result r);

    /// Verify `core`, reading it through `file`.
    ///
    /// `file` must be a handle to `core` opened with write sharing denied —
    /// both the signature check and the Authenticode check run against that
    /// handle, so the bytes verified are the bytes that stay on disk while we
    /// inject.
    Result verify(HANDLE file, const std::filesystem::path &core);
}
