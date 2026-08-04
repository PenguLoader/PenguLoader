<#
.SYNOPSIS
  Write (or check) the Pengu signature embedded in core.dll's `.pengu` section.

.DESCRIPTION
  boot.dll refuses to inject a core it can't verify. The signature lives inside
  core.dll rather than in a sidecar manifest, which means this runs BEFORE the
  file goes for Authenticode code signing — the reverse of the old
  pengu.manifest ordering, and the reason that ordering constraint is gone.

  The signed region is the whole file from byte 0 to
  `SizeOfHeaders + sum(SizeOfRawData)`, with three holes:

      OptionalHeader.CheckSum                          4 bytes
      DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY]    8 bytes
      the 64-byte signature field itself

  The first two are exactly what signtool rewrites. The upper bound is derived
  from the headers rather than being the file length because everything past
  the last section's raw data belongs to signtool — alignment padding and the
  appended certificate table — so the digest comes out bit-identical before and
  after code signing.

  This file and boot/trust.cc are two halves of one definition. A disagreement
  between them ships as a release that silently refuses to activate, which is
  what -Verify exists to catch.

.PARAMETER Core
  Path to core.dll, or to a directory containing it.

.PARAMETER Key
  Private key as base64 PKCS#8, or a path to a file containing it. Defaults to
  the PENGU_TRUST_KEY environment variable so CI can pass it as a secret and it
  never reaches a command line.

.PARAMETER Verify
  Check the embedded signature instead of writing one. Needs -PublicKey.

.PARAMETER PublicKey
  Hex or comma-separated bytes of the public point(s), for -Verify. Paste
  straight out of boot/trust_key.h: 64 bytes for the primary key alone, or 128
  when a successor has been announced, in which case either is accepted.

.EXAMPLE
  ./build/sign-core.ps1 -Core bin/
  # then send bin/core.dll for Authenticode signing

.EXAMPLE
  $pub = (Select-String boot/trust_key.h -Pattern '0x[0-9A-F]{2}' -AllMatches |
          ForEach-Object { $_.Matches.Value }) -join ''
  ./build/sign-core.ps1 -Core signed/ -Verify -PublicKey $pub
#>
param(
  [Parameter(Mandatory)] [string] $Core,
  [string] $Key,
  [switch] $Verify,
  [string] $PublicKey
)

$ErrorActionPreference = 'Stop'

$MAGIC       = [byte[]] @(0x50,0x45,0x4E,0x47,0x55,0x53,0x49,0x47)  # "PENGUSIG"
$SECTION     = [byte[]] @(0x2E,0x70,0x65,0x6E,0x67,0x75,0x00,0x00)  # ".pengu\0\0"
$BLOB_SIZE   = 512
$BLOB_SIG    = 16   # signature field, within the blob
$SIG_SIZE    = 64
$FORMAT      = 1

if (Test-Path -LiteralPath $Core -PathType Container) {
  $Core = Join-Path $Core 'core.dll'
}
if (-not (Test-Path -LiteralPath $Core -PathType Leaf)) { throw "no core.dll at $Core" }
$Core = (Resolve-Path -LiteralPath $Core).Path

# ---------------------------------------------------------------------------

function Test-Bytes([byte[]] $Buffer, [int] $At, [byte[]] $Expected) {
  for ($i = 0; $i -lt $Expected.Length; $i++) {
    if ($Buffer[$At + $i] -ne $Expected[$i]) { return $false }
  }
  return $true
}

# Locate the four things the digest depends on. Mirrors locate() in trust.cc;
# every rejection here is one the boot would also make, just louder and at
# build time instead of on a user's machine.
function Get-PeLayout([byte[]] $f) {
  if ($f.Length -lt 0x40 -or $f[0] -ne 0x4D -or $f[1] -ne 0x5A) { throw 'not a PE (no MZ)' }

  $lfanew = [int][BitConverter]::ToUInt32($f, 0x3C)
  if ($lfanew -le 0 -or $lfanew + 24 -gt $f.Length) { throw 'not a PE (bad e_lfanew)' }
  if ([BitConverter]::ToUInt32($f, $lfanew) -ne 0x00004550) { throw 'not a PE (no PE signature)' }

  $fileHeader   = $lfanew + 4
  $sectionCount = [int][BitConverter]::ToUInt16($f, $fileHeader + 2)
  $optionalSize = [int][BitConverter]::ToUInt16($f, $fileHeader + 16)
  $optional     = $fileHeader + 20

  if ([BitConverter]::ToUInt16($f, $optional) -ne 0x20B) { throw 'not PE32+ (only x64 cores are shipped)' }
  if ($optionalSize -lt 152) { throw 'optional header too small to hold the security directory' }
  if ([BitConverter]::ToUInt32($f, $optional + 108) -le 4) { throw 'no security data directory entry' }

  $signedEnd = [int64][BitConverter]::ToUInt32($f, $optional + 60)   # SizeOfHeaders
  $table     = $optional + $optionalSize
  $blob      = -1

  for ($i = 0; $i -lt $sectionCount; $i++) {
    $header = $table + $i * 40
    if ($header + 40 -gt $f.Length) { throw 'section table runs past the end of the file' }

    $rawSize    = [int64][BitConverter]::ToUInt32($f, $header + 16)
    $rawPointer = [int64][BitConverter]::ToUInt32($f, $header + 20)
    $signedEnd += $rawSize

    if (-not (Test-Bytes $f $header $SECTION)) { continue }
    if ($blob -ge 0) { throw 'two .pengu sections' }
    if ($rawSize -lt $BLOB_SIZE -or $rawPointer + $BLOB_SIZE -gt $f.Length) {
      throw '.pengu section is too small to hold the signature block'
    }
    $blob = [int]$rawPointer
  }

  if ($blob -lt 0) {
    throw "core.dll has no .pengu section. Is core/src/signature.cc in the build, and did the linker keep it?"
  }
  if ($signedEnd -gt $f.Length) { throw 'section raw data runs past the end of the file' }

  if (-not (Test-Bytes $f $blob $MAGIC)) { throw '.pengu section does not start with the signature block' }
  $found = [BitConverter]::ToUInt32($f, $blob + 8)
  if ($found -ne $FORMAT) { throw "signature block format $found, expected $FORMAT" }

  [pscustomobject]@{
    Checksum  = $optional + 64
    Security  = $optional + 144
    Signature = $blob + $BLOB_SIG
    SignedEnd = [int]$signedEnd
  }
}

# The exact byte sequence boot.dll hashes.
function Get-SignedBytes([byte[]] $f, $l) {
  $out = New-Object System.IO.MemoryStream
  $out.Write($f, 0,                        $l.Checksum)
  $out.Write($f, $l.Checksum + 4,          $l.Security  - ($l.Checksum + 4))
  $out.Write($f, $l.Security + 8,          $l.Signature - ($l.Security + 8))
  $out.Write($f, $l.Signature + $SIG_SIZE, $l.SignedEnd - ($l.Signature + $SIG_SIZE))
  $out.ToArray()
}

function ConvertTo-KeyBytes([string] $Text) {
  $hex = $Text -replace '0x|,|\s', ''
  if ($hex.Length % 2 -ne 0) { throw 'public key is not a whole number of bytes' }
  ,[byte[]] (($hex -split '(?<=\G..)(?=.)') | ForEach-Object { [Convert]::ToByte($_, 16) })
}

function New-PublicEcdsa([byte[]] $Point) {
  $ec = [System.Security.Cryptography.ECDsa]::Create()
  $ec.ImportParameters([System.Security.Cryptography.ECParameters]@{
    Curve = [System.Security.Cryptography.ECCurve]::CreateFromFriendlyName('nistP256')
    Q     = [System.Security.Cryptography.ECPoint]@{ X = $Point[0..31]; Y = $Point[32..63] }
  })
  $ec
}

# ---------------------------------------------------------------------------

$bytes  = [System.IO.File]::ReadAllBytes($Core)
$layout = Get-PeLayout $bytes
$body   = Get-SignedBytes $bytes $layout

if ($Verify) {
  if (-not $PublicKey) { throw '-Verify needs -PublicKey' }

  $point = ConvertTo-KeyBytes $PublicKey
  if ($point.Length -ne 64 -and $point.Length -ne 128) {
    throw "public key must be 64 bytes (primary) or 128 (primary + successor), got $($point.Length)"
  }

  $signature = $bytes[$layout.Signature..($layout.Signature + $SIG_SIZE - 1)]
  if (-not ($signature | Where-Object { $_ -ne 0 })) {
    "core.dll : $Core"
    'signature: ABSENT — this core was never signed and cannot be activated'
    exit 1
  }

  $slot = 0
  for ($at = 0; $at -lt $point.Length; $at += 64) {
    $ec = New-PublicEcdsa $point[$at..($at + 63)]
    if ($ec.VerifyData($body, $signature, [System.Security.Cryptography.HashAlgorithmName]::SHA256)) {
      "core.dll : $Core"
      "signature: valid (key slot $slot, $($body.Length) bytes covered)"
      exit 0
    }
    $slot++
  }

  "core.dll : $Core"
  'signature: INVALID — not made by any key this boot trusts'
  exit 1
}

if (-not $Key) { $Key = $env:PENGU_TRUST_KEY }
if (-not $Key) { throw 'no signing key: pass -Key or set PENGU_TRUST_KEY' }
if (Test-Path -LiteralPath $Key) { $Key = (Get-Content -LiteralPath $Key -Raw) }

# Trailing data past the last section is not covered by us. Authenticode does
# cover it, so this is a note rather than a failure — but it should be zero for
# a normal linker-produced DLL, and if it isn't, someone should know why.
if ($layout.SignedEnd -ne $bytes.Length) {
  Write-Warning "$($bytes.Length - $layout.SignedEnd) bytes past the last section are outside the signed region"
}

$ec = [System.Security.Cryptography.ECDsa]::Create()
$ec.ImportPkcs8PrivateKey([Convert]::FromBase64String($Key.Trim()), [ref]0)

# .NET's ECDsa.SignData emits raw r||s (IEEE P1363), which is exactly what
# BCryptVerifySignature expects — no DER unwrapping on either side.
$signature = $ec.SignData($body, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
if ($signature.Length -ne $SIG_SIZE) { throw "expected a $SIG_SIZE-byte signature, got $($signature.Length)" }

[Array]::Copy($signature, 0, $bytes, $layout.Signature, $SIG_SIZE)
[System.IO.File]::WriteAllBytes($Core, $bytes)

"signed $Core"
"  covered $($body.Length) bytes, signature at offset $($layout.Signature)"
'  send it for Authenticode signing now — the digest is unaffected by that'
