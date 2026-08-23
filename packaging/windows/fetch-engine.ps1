<#
.SYNOPSIS
    Fetch the audio engine SqeezeAmp drives.

.DESCRIPTION
    SqeezeAmp plays audio by supervising a stock, unmodified squeezelite.exe as
    a child process. That binary is GPLv3 and is deliberately not distributed
    with SqeezeAmp — see THIRD-PARTY-NOTICES.md. This script downloads it from
    upstream, checks it against the pinned SHA-256, and puts it where
    ExternalEngine looks: engine\squeezelite.exe.

    The installer does the same thing during setup. This script is for the
    portable zip, which has no installer to do it, and for repairing an
    install whose engine went missing.

    The download location is not baked in here either. It comes from
    packaging/engine-manifest.txt, read over the network by default, because
    upstream prunes old builds from SourceForge and a hard-coded URL would
    eventually rot. See that file for the whole reasoning.

.PARAMETER DestDir
    Where to write squeezelite.exe. Defaults to engine\ beside this script.

.PARAMETER ManifestUrl
    Where to read the manifest from. The default is this project's own copy on
    GitHub, which is what makes a pruned upstream build repairable.

.PARAMETER ManifestFile
    A local manifest to use instead. Implied by -Offline.

.PARAMETER Offline
    Do not fetch the manifest over the network; use the local copy beside this
    script. The engine download itself still needs the network.

.PARAMETER Force
    Overwrite an existing engine\squeezelite.exe. Without this, an engine that
    is already present and already matches the pinned checksum is left alone.
#>
[CmdletBinding()]
param(
    [string] $DestDir,
    [string] $ManifestUrl = 'https://raw.githubusercontent.com/kvit-s/sqeezeamp/main/packaging/engine-manifest.txt',
    [string] $ManifestFile,
    [switch] $Offline,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 still defaults to SSL3/TLS1.0 on some machines, and
# both raw.githubusercontent.com and SourceForge refuse those outright. The
# failure looks like "the underlying connection was closed", which says nothing
# about the cause.
try {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }

# Not a browser. SourceForge answers browser-shaped User-Agents with an HTML
# "your download will start shortly" page instead of the file.
$UserAgent = 'SqeezeAmp-fetch-engine'

function Read-Manifest {
    $text = $null

    if (-not $Offline -and -not $ManifestFile) {
        try {
            $text = (Invoke-WebRequest -Uri $ManifestUrl -UseBasicParsing `
                        -UserAgent $UserAgent -TimeoutSec 30).Content
            Write-Host "manifest: $ManifestUrl"
        } catch {
            Write-Warning "could not read the manifest from $ManifestUrl -- falling back to the local copy"
            Write-Warning "  $($_.Exception.Message)"
        }
    }

    if (-not $text) {
        # Beside this script in a staged/portable tree; one directory up in the
        # source tree, where it lives at packaging\engine-manifest.txt.
        $candidates = @()
        if ($ManifestFile) { $candidates += $ManifestFile }
        $candidates += (Join-Path $PSScriptRoot 'engine-manifest.txt')
        $candidates += (Join-Path (Split-Path $PSScriptRoot -Parent) 'engine-manifest.txt')

        foreach ($c in $candidates) {
            if (Test-Path -LiteralPath $c) {
                $text = Get-Content -LiteralPath $c -Raw
                Write-Host "manifest: $c"
                break
            }
        }
    }

    if (-not $text) { throw "no engine manifest available, locally or at $ManifestUrl" }

    $map = @{}
    foreach ($line in ($text -split "`r?`n")) {
        $stripped = ($line -split '#', 2)[0].Trim()
        if (-not $stripped) { continue }
        $kv = $stripped -split '=', 2
        if ($kv.Count -eq 2) { $map[$kv[0].Trim().ToLower()] = $kv[1].Trim() }
    }
    foreach ($required in @('version', 'url', 'sha256', 'member')) {
        if (-not $map.ContainsKey($required)) { throw "the manifest has no '$required' line" }
    }
    return $map
}

function Get-Sha256([string] $Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLower()
}

# ── Where the engine goes.
if (-not $DestDir) { $DestDir = Join-Path $PSScriptRoot 'engine' }
$enginePath = Join-Path $DestDir 'squeezelite.exe'

$manifest = Read-Manifest
Write-Host "engine  : squeezelite $($manifest.version)"

if ((Test-Path -LiteralPath $enginePath) -and -not $Force) {
    $have = Get-Sha256 $enginePath
    if ($manifest.ContainsKey('member_sha256') -and $have -eq $manifest.member_sha256) {
        Write-Host "already present and matching at $enginePath -- nothing to do"
        exit 0
    }
    Write-Warning "an engine is already at $enginePath but is not the pinned build; use -Force to replace it"
    exit 2
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("sqz-engine-" + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work | Out-Null

try {
    $zip = Join-Path $work 'engine.zip'
    Write-Host "download: $($manifest.url)"
    Invoke-WebRequest -Uri $manifest.url -OutFile $zip -UseBasicParsing `
        -UserAgent $UserAgent -MaximumRedirection 10 -TimeoutSec 300

    # Check the shape before the checksum. A mirror that answers with an HTML
    # error page produces a checksum mismatch, which reads as "the file was
    # tampered with" when the truth is "that is not a file".
    $head = New-Object byte[] 2
    $fs = [System.IO.File]::OpenRead($zip)
    try { [void]$fs.Read($head, 0, 2) } finally { $fs.Dispose() }
    if ($head[0] -ne 0x50 -or $head[1] -ne 0x4B) {
        throw ("the download is not a zip archive: {0} bytes starting '{1}'. " +
               "The URL in the manifest may have been pruned upstream." -f `
               (Get-Item $zip).Length, [System.Text.Encoding]::ASCII.GetString($head))
    }

    $got = Get-Sha256 $zip
    if ($got -ne $manifest.sha256.ToLower()) {
        throw "checksum mismatch`n  expected $($manifest.sha256)`n  got      $got"
    }
    Write-Host "sha256  : ok"

    $unpacked = Join-Path $work 'unpacked'
    Expand-Archive -LiteralPath $zip -DestinationPath $unpacked -Force

    $member = Join-Path $unpacked $manifest.member
    if (-not (Test-Path -LiteralPath $member)) {
        throw "the archive does not contain $($manifest.member)"
    }
    if ($manifest.ContainsKey('member_sha256')) {
        $gotMember = Get-Sha256 $member
        if ($gotMember -ne $manifest.member_sha256.ToLower()) {
            throw "the executable inside the archive does not match member_sha256`n  expected $($manifest.member_sha256)`n  got      $gotMember"
        }
    }

    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    Copy-Item -LiteralPath $member -Destination $enginePath -Force

    # Upstream ships its licence text in the archive. It costs nothing to keep
    # it next to the binary, and a user who now has a GPLv3 program on disk
    # should have its terms too.
    $upstreamLicence = Join-Path $unpacked 'LICENSE.txt'
    if (Test-Path -LiteralPath $upstreamLicence) {
        Copy-Item -LiteralPath $upstreamLicence `
                  -Destination (Join-Path $DestDir 'LICENSE.squeezelite.txt') -Force
    }

    Write-Host "engine  : $enginePath"
    exit 0
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
