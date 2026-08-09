# Stage the MSVC C runtime beside the built sqeezeamp.exe.
#
# windeployqt brings Qt, not the C runtime the app was compiled against. On the
# machine that built it that goes unnoticed, because Visual Studio put the CRT
# on the system; on a clean machine the app dies before main() with a missing
# vcruntime140.dll and no window to say so. The portable zip has no installer
# to run a redist for it either, so the DLLs travel in the tree.
#
# Bundle the newest redist across every Visual Studio install: the C runtime's
# compatibility rule is that a runtime at least as new as the toolset that built
# the binary is safe, so the highest version present is always correct. This
# walks the install tree rather than trusting vswhere, which on a machine
# carrying both VS 2022 (VC143) and VS 2019 Build Tools (VC142) resolves to the
# older one. Only numeric `x.y.z` version directories count — the `vNNN` aliases
# are the same files under another name — and DebugCRT is excluded, because
# shipping the debug runtime needs a licence this project does not have.
param([Parameter(Mandatory = $true)][string]$StageDir)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $StageDir)) { throw "stage-crt: no staged tree at $StageDir" }

$vsRoots = @(
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio')
) | Where-Object { Test-Path $_ }

$candidates = foreach ($root in $vsRoots) {
    foreach ($year in Get-ChildItem $root -Directory -ErrorAction SilentlyContinue) {
        foreach ($edition in Get-ChildItem $year.FullName -Directory -ErrorAction SilentlyContinue) {
            $redist = Join-Path $edition.FullName 'VC\Redist\MSVC'
            if (-not (Test-Path $redist)) { continue }
            Get-ChildItem $redist -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
                ForEach-Object {
                    $crt = Get-ChildItem (Join-Path $_.FullName 'x64') -Directory `
                            -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -notmatch 'Debug' } | Select-Object -First 1
                    if ($crt) { [pscustomobject]@{ Ver = [version]$_.Name; Dir = $crt } }
                }
        }
    }
}

if (-not $candidates) {
    throw 'stage-crt: no Microsoft.VC*.CRT redist found in any Visual Studio install.'
}

$best = $candidates | Sort-Object Ver -Descending | Select-Object -First 1
Copy-Item (Join-Path $best.Dir.FullName '*.dll') $StageDir -Force
Write-Host "win-deploy: staged the MSVC C runtime $($best.Dir.Name) (redist $($best.Ver))"
