<#
  package.ps1 - Assemble a portable, no-install Windows bundle of custom_infill_slicer
  (a PrusaSlicer fork) for sharing, optionally including a printer profile.

  All paths are derived from the script's own location or auto-discovered from the build,
  so nothing machine-specific is baked in. Override any of them with parameters if needed.

  Examples:
      .\package.ps1
      .\package.ps1 -ProfileSrc ..\3D45-Profile-for-PrusaSlicer
      .\package.ps1 -Config Release -OutDir D:\out

  Output:  <OutDir>\custom_infill_slicer-<version>-win64.zip
           (OutDir defaults to a 'cis-dist' folder next to the repo)
#>
param(
    [string]$Config     = "RelWithDebInfo",
    [string]$Arch       = "x64",
    [string]$Repo       = $PSScriptRoot,
    [string]$DepsBin    = "",   # auto-discovered from the build's deps-path cache when empty
    [string]$ProfileSrc = "",   # optional folder with 3D45.ini + Dremel_3D45_platform.stl; skipped if empty/missing
    [string]$OutDir     = ""    # defaults to a 'cis-dist' folder next to the repo
)

$ErrorActionPreference = "Stop"

if (-not $Repo)   { $Repo = (Get-Location).Path }
if (-not $OutDir) { $OutDir = Join-Path (Split-Path -Parent $Repo) "cis-dist" }

$binDir = Join-Path $Repo "build\src\$Config"
$resDir = Join-Path $Repo "resources"

# Auto-discover the deps destdir 'bin' from the cache build_win.bat writes, unless given.
if (-not $DepsBin) {
    $cache = Join-Path $Repo "build\.vs\$Arch\$Config\.DEPS_PATH.txt"
    if (Test-Path $cache) {
        $destdir = (Get-Content $cache -TotalCount 1).Trim()
        if ($destdir) { $DepsBin = Join-Path $destdir "usr\local\bin" }
    }
}
if (-not $DepsBin -or -not (Test-Path $DepsBin)) {
    throw "Could not locate the deps 'bin' folder. Pass -DepsBin '<destdir>\usr\local\bin' explicitly."
}

foreach ($p in @($binDir, $resDir, $DepsBin)) {
    if (-not (Test-Path $p)) { throw "Required path not found: $p" }
}

# --- derive version from the console binary (e.g. "PrusaSlicer-2.9.6-rc1+UNKNOWN ...") ---
$consoleExe = Join-Path $binDir "prusa-slicer-console.exe"
$verLine = & $consoleExe --help 2>$null | Select-Object -First 1
$ver = if ($verLine -match "PrusaSlicer-([0-9][^\s+]*)") { $Matches[1] } else { "unknown" }

$stageName = "custom_infill_slicer-$ver-win64"
$stage = Join-Path $OutDir $stageName
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Write-Host "Staging -> $stage  (version $ver)"

# --- 1. application binaries (exclude dev artifacts: .pdb / .lib / .exp) ---
$appFiles = @(
    "prusa-slicer.exe", "prusa-slicer-console.exe", "prusa-gcodeviewer.exe",
    "PrusaSlicer.dll", "OCCTWrapper.dll",
    "libgmp-10.dll", "libmpfr-4.dll", "WebView2Loader.dll"
)
foreach ($f in $appFiles) { Copy-Item (Join-Path $binDir $f) $stage }

# --- 2. MSVC C++ runtime (app-local => recipients need no VC++ redistributable installed) ---
$crt = @(
    "vcruntime140.dll", "vcruntime140_1.dll",
    "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
    "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "concrt140.dll"
)
foreach ($f in $crt) { Copy-Item (Join-Path $DepsBin $f) $stage }

# --- 3. resources tree (REQUIRED - the slicer will not start without it) ---
Copy-Item $resDir (Join-Path $stage "resources") -Recurse

# --- 4. optional printer profile + instructions ---
if ($ProfileSrc -and (Test-Path (Join-Path $ProfileSrc "3D45.ini"))) {
    $pp = Join-Path $stage "printer-profile"
    New-Item -ItemType Directory -Force -Path $pp | Out-Null
    if (Test-Path (Join-Path $ProfileSrc "Dremel_3D45_platform.stl")) {
        Copy-Item (Join-Path $ProfileSrc "Dremel_3D45_platform.stl") $pp
    }
    # Blank the broken absolute bed_custom_model path (it pointed at the author's Cura
    # install); the bed *size* still imports and the custom mesh is loaded manually.
    (Get-Content (Join-Path $ProfileSrc "3D45.ini")) `
        -replace '^(bed_custom_model\s*=).*$', '$1 ' |
        Set-Content (Join-Path $pp "3D45.ini") -Encoding UTF8
    @"
Dremel 3D45 printer profile - custom_infill_slicer
==================================================
1. Launch prusa-slicer.exe (one folder up from here).
2. File > Import > Import Config...  ->  select  printer-profile\3D45.ini
   This loads all printer settings and the correct 255 x 155 mm bed.
3. (Optional) custom 3D bed model - cosmetic; the bed size is already correct:
   Printer Settings tab > General > Bed shape "Set..." > Custom model
   ->  pick  printer-profile\Dremel_3D45_platform.stl
"@ | Set-Content (Join-Path $pp "READ-ME-FIRST.txt") -Encoding UTF8
    Write-Host "Included printer profile from $ProfileSrc"
} else {
    Write-Host "No printer profile included (pass -ProfileSrc <folder> to add one)."
}

# --- 5. top-level run note ---
@"
custom_infill_slicer $ver  (Windows x64, portable)

Double-click prusa-slicer.exe to run - no installation required.
"@ | Set-Content (Join-Path $stage "READ-ME.txt") -Encoding UTF8

# --- 6. zip it ---
$zip = Join-Path $OutDir "$stageName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal

Write-Host ""
Write-Host "Bundle ready: $zip"
Write-Host ("Zip size:    {0:N1} MB" -f ((Get-Item $zip).Length / 1MB))
