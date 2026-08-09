# Write SHA256SUMS-windows.txt for the artifacts in dist\.
#
# The format is the one `sha256sum -c` reads: lowercase hash, two spaces, bare
# filename. It is what a download can be verified against, and what tells two
# builds of the same version apart when one of them is stale.
#
# The hashing goes through .NET rather than Get-FileHash on purpose. Get-FileHash
# lives in a module, and Windows PowerShell resolves modules through
# PSModulePath — which is inherited, so launching this from a PowerShell 7 shell
# (or from a batch file that was itself started from one) can shadow the 5.1
# Utility module and leave Get-FileHash undefined. SHA256 from the framework has
# no such dependency and behaves the same in every host.
param([Parameter(Mandatory = $true)][string]$DistDir)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $DistDir)) { throw "checksums: no dist directory at $DistDir" }

$artifacts = Get-ChildItem -Path $DistDir -File |
    Where-Object { $_.Extension -in '.zip', '.exe' } |
    Sort-Object Name

if (-not $artifacts) { throw "checksums: no .zip or .exe artifacts in $DistDir" }

$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $lines = foreach ($file in $artifacts) {
        $stream = [System.IO.File]::OpenRead($file.FullName)
        try { $hash = $sha.ComputeHash($stream) } finally { $stream.Dispose() }
        '{0}  {1}' -f [System.BitConverter]::ToString($hash).Replace('-', '').ToLowerInvariant(), $file.Name
    }
} finally {
    $sha.Dispose()
}

$out = Join-Path $DistDir 'SHA256SUMS-windows.txt'
Set-Content -Path $out -Value $lines -Encoding ascii
Write-Host "win-package: $out"
