#pragma once

// The trust anchor: the public half of the key that signs release cores.
//
// This is the ONE thing the boot must never get wrong, and the one thing that
// can't be rotated by shipping a new core — replacing it means replacing the
// boot, which means an admin prompt on every machine. Everything else about
// the trust chain is designed so this key almost never has to change:
//
//   * The code-signing certificate is deliberately NOT the anchor. SignPath
//     renewals, a CA change, or moving to our own certificate are all
//     invisible here.
//   * There are two slots, so a rotation can be announced a release ahead of
//     being used. See below — this is not optional discipline.
//
// ---------------------------------------------------------------------------
// ROTATING
//
// boot.dll and core.dll drift by design. The boot is installed once into
// %ProgramData%\.pengu and a portable core update never touches it, so at any
// moment there are older boots in the field than the core being shipped. A
// rotation therefore has to run FORWARD: publish the successor first, start
// signing with it later.
//
//   Release N     PENGU_TRUST_PUBKEY = old, PENGU_TRUST_PUBKEY_NEXT = new.
//                 Cores are still signed with the old key. Every boot from
//                 this release onwards will accept either.
//   Release N+1   Cores are signed with the new key. Boots from release N or
//                 later keep working; nothing needs replacing.
//   Release N+2   PENGU_TRUST_PUBKEY = new, PENGU_TRUST_PUBKEY_NEXT = zeroed.
//                 The old key is out of the field.
//
// Skipping straight to signing with a key no shipped boot knows leaves every
// install whose boot predates the rotation refusing the new core. The client
// still launches, plugins silently don't, and the only clue is the refusal
// line in Settings. Don't.
//
// A rollover record carried inside the core and signed by the outgoing key was
// considered and rejected: it turns the anchor into a chain, so whoever
// compromises one key can mint successors indefinitely, and it can't help
// after a key is lost anyway — the record only travels inside a core the old
// key signed.
// ---------------------------------------------------------------------------
// DEVELOPMENT KEY — REPLACE BEFORE RELEASE
//
// Generated locally for testing. The matching private key has never been in
// the repository and is not the release key. Release tooling must generate the
// real keypair in the signing environment, replace the bytes below, and set
// PENGU_TRUST_KEY_IS_DEV to false.
// ---------------------------------------------------------------------------

#include <cstdint>

// Raw ECDSA P-256 public point, X || Y, 32 bytes each. This is the payload of
// a BCRYPT_ECCKEY_BLOB; the header is built at import time.
inline constexpr uint8_t PENGU_TRUST_PUBKEY[64] = {
    0x93, 0xE3, 0x2C, 0x75, 0xAB, 0xF4, 0x14, 0x87, 0xC7, 0x85, 0x6A, 0x49,
    0x42, 0x16, 0x39, 0x95, 0x96, 0xA9, 0x08, 0x1A, 0xBC, 0xC4, 0x40, 0x96,
    0xA2, 0xE4, 0x1A, 0x43, 0xFE, 0xEC, 0x97, 0x26, 0x77, 0xEE, 0x3B, 0x0E,
    0xD7, 0x79, 0xEC, 0x9F, 0xDE, 0x88, 0x73, 0x79, 0x00, 0x08, 0xF3, 0x7B,
    0xA1, 0x14, 0x40, 0x5B, 0x14, 0x6C, 0x20, 0x88, 0xCF, 0x36, 0xFC, 0x15,
    0x3F, 0xD0, 0x06, 0xB9,
};

// The announced successor, accepted alongside the primary. All zeroes means
// there isn't one and the verifier skips the slot entirely — that is the
// steady state, not a placeholder to be filled in casually.
inline constexpr uint8_t PENGU_TRUST_PUBKEY_NEXT[64] = {};

// True while the development key above is still in place. The boot logs a
// warning so a debug build never quietly looks like a release one.
inline constexpr bool PENGU_TRUST_KEY_IS_DEV = true;
