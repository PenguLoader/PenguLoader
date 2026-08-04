<#
.SYNOPSIS
  Generate the ECDSA P-256 keypair that signs release cores.

.DESCRIPTION
  The public half is compiled into boot.dll (boot/trust_key.h) and is the
  boot's trust anchor — the thing that decides which core.dll may be loaded.
  The private half signs core.dll's `.pengu` section via build/sign-core.ps1.

  It is deliberately NOT the code-signing certificate. That certificate belongs
  to SignPath Foundation, is shared with unrelated open-source projects, and
  renews on someone else's schedule; pinning it would make every renewal a
  boot-replacement event needing admin on every machine. This key is ours and
  rotates when we decide.

  ROTATING runs forward, in three releases — publish the successor, then start
  signing with it, then retire the old one. boot.dll and core.dll drift by
  design, so at any moment there are older boots in the field than the core
  being shipped, and a hard cutover leaves them refusing it. trust_key.h spells
  out the sequence; use -Successor for step one.

.PARAMETER Out
  Where to write the private key (base64 PKCS#8). Defaults to stdout only.

.PARAMETER Successor
  Format the public half for PENGU_TRUST_PUBKEY_NEXT rather than for the
  primary slot. This is the first step of a rotation.

.EXAMPLE
  ./build/new-trust-key.ps1
  # paste the C array into boot/trust_key.h, store the private key as a secret
#>
param(
  [string] $Out,
  [switch] $Successor
)

$ErrorActionPreference = 'Stop'

$ec = [System.Security.Cryptography.ECDsa]::Create(
  [System.Security.Cryptography.ECCurve]::CreateFromFriendlyName('nistP256'))

$params  = $ec.ExportParameters($true)
$private = [Convert]::ToBase64String($ec.ExportPkcs8PrivateKey())

# BCryptImportKeyPair wants the raw public point, X || Y, 32 bytes each.
$point = $params.Q.X + $params.Q.Y
$slot  = if ($Successor) { 'PENGU_TRUST_PUBKEY_NEXT' } else { 'PENGU_TRUST_PUBKEY' }

$lines = for ($i = 0; $i -lt $point.Length; $i += 12) {
  $chunk = $point[$i..([Math]::Min($i + 11, $point.Length - 1))]
  '    ' + (($chunk | ForEach-Object { '0x{0:X2}' -f $_ }) -join ', ') + ','
}

Write-Host ''
Write-Host "=== public key — replace $slot in boot/trust_key.h ===" -ForegroundColor Cyan
Write-Host ''
$lines | ForEach-Object { Write-Host $_ }
Write-Host ''

if ($Successor) {
  Write-Host 'Ship a release with this in the successor slot BEFORE signing any' -ForegroundColor Cyan
  Write-Host 'core with the matching private key. Boots older than that release' -ForegroundColor Cyan
  Write-Host 'will not know the key and will refuse the core.' -ForegroundColor Cyan
} else {
  Write-Host 'and set PENGU_TRUST_KEY_IS_DEV to false.' -ForegroundColor Cyan
}

Write-Host ''
Write-Host '=== private key — store as the PENGU_TRUST_KEY secret ===' -ForegroundColor Yellow
Write-Host ''

if ($Out) {
  [System.IO.File]::WriteAllText($Out, $private)
  Write-Host "  written to $Out" -ForegroundColor Yellow
  Write-Host '  Do not commit this. Delete it once it is in the secret store.' -ForegroundColor Yellow
} else {
  Write-Host "  $private"
}

Write-Host ''
Write-Host 'Losing the private key is recoverable but expensive: every boot in the' -ForegroundColor DarkGray
Write-Host 'field trusts only these keys, and a lost key cannot be rotated forward' -ForegroundColor DarkGray
Write-Host 'because the successor slot has to ship before it is used. Recovery' -ForegroundColor DarkGray
Write-Host 'means a new boot on every machine, each with an admin prompt. Back it up.' -ForegroundColor DarkGray
