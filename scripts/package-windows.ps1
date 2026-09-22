# Poppy Standalone Windows Release Packaging Script
# Uses windeployqt to bundle required Qt runtime DLLs, plugins, and dependencies

param(
    [string]$BuildDir = "$PSScriptRoot/../build",
    [string]$DistDir = "$PSScriptRoot/../dist/Poppy",
    [string]$ZipOutput = "$PSScriptRoot/../dist/Poppy-windows-x64.zip",
    [string]$QtBin = "C:\Qt\6.10.1\mingw_64\bin",
    [string]$MinGwBin = "C:\Qt\Tools\mingw1310_64\bin"
)

$ErrorActionPreference = "Stop"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Poppy Windows Release Packager" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# 1. Check tools
$env:PATH = "$MinGwBin;$QtBin;" + $env:PATH
$windeployqt = Join-Path $QtBin "windeployqt.exe"

if (-not (Test-Path $windeployqt)) {
    Write-Warning "windeployqt.exe not found at $windeployqt. Falling back to PATH lookup."
    $windeployqt = "windeployqt"
}

# 2. Verify binaries
$poppyExe = Join-Path $BuildDir "bin/poppy.exe"
$cliExe = Join-Path $BuildDir "bin/poppy-cli.exe"

if (-not (Test-Path $poppyExe)) {
    throw "poppy.exe not found at $poppyExe. Please run cmake --build build first."
}

# 3. Clean and create Dist directory
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Write-Host "Copying executables..." -ForegroundColor Green
Copy-Item $poppyExe -Destination $DistDir
if (Test-Path $cliExe) {
    Copy-Item $cliExe -Destination $DistDir
}

# 4. Run windeployqt
Write-Host "Deploying Qt dependencies via windeployqt..." -ForegroundColor Green
& $windeployqt "$DistDir/poppy.exe" --no-translations --compiler-runtime

# 5. Copy extra assets & documentation
$rootDir = (Resolve-Path "$PSScriptRoot/..").Path
Write-Host "Copying documentation and assets..." -ForegroundColor Green
if (Test-Path "$rootDir/README.md") { Copy-Item "$rootDir/README.md" $DistDir }
if (Test-Path "$rootDir/LICENSE") { Copy-Item "$rootDir/LICENSE" $DistDir }
if (Test-Path "$rootDir/assets") { Copy-Item -Recurse "$rootDir/assets" $DistDir }
if (Test-Path "$rootDir/examples") { Copy-Item -Recurse "$rootDir/examples" $DistDir }

# 6. Compress to ZIP (Portable)
if (Test-Path $ZipOutput) {
    Remove-Item -Force $ZipOutput
}
Write-Host "Compressing to $ZipOutput..." -ForegroundColor Green
Compress-Archive -Path "$DistDir/*" -DestinationPath $ZipOutput -Force

$zipSize = (Get-Item $ZipOutput).Length / 1MB
Write-Host "Portable ZIP created: $ZipOutput ($([math]::Round($zipSize, 2)) MB)" -ForegroundColor Yellow

# 7. Compile Windows Setup Installer (Inno Setup)
$isccPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $isccPath)) {
    $isccCmd = Get-Command iscc -ErrorAction SilentlyContinue
    if ($isccCmd) { $isccPath = $isccCmd.Source }
}

if (Test-Path $isccPath) {
    Write-Host "Compiling Windows Installer with Inno Setup..." -ForegroundColor Green
    & $isccPath "$rootDir/installer/poppy_setup.iss"
    $installerExe = "$rootDir/release-installer/Poppy-windows-x64-setup.exe"
    if (Test-Path $installerExe) {
        $setupSize = (Get-Item $installerExe).Length / 1MB
        Write-Host "Windows Installer created: $installerExe ($([math]::Round($setupSize, 2)) MB)" -ForegroundColor Yellow
    }
} else {
    Write-Warning "Inno Setup (ISCC.exe) not found. Skipping installer generation."
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Windows Packaging completed successfully!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
