# Script de build de MoteurJV (Windows / MSVC ARM64).
# Usage :  .\build.ps1            (configure + compile en Debug)
#          .\build.ps1 -Run       (compile puis lance la démo)
#          .\build.ps1 -Config Release

param(
    [string]$Config = "Debug",
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# Trouver cmake : d'abord celui du PATH, sinon celui bundlé avec VS Build Tools.
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { $cmake = $c; break } }
}
if (-not $cmake) { throw "cmake introuvable. L'installation de MSVC Build Tools est-elle terminée ?" }

Write-Host "==> cmake : $cmake" -ForegroundColor Cyan

# Configuration (génère le projet Visual Studio, architecture ARM64).
& $cmake -S $root -B "$root\build" -G "Visual Studio 17 2022" -A ARM64
if ($LASTEXITCODE -ne 0) { throw "Echec de la configuration CMake." }

# Compilation.
& $cmake --build "$root\build" --config $Config
if ($LASTEXITCODE -ne 0) { throw "Echec de la compilation." }

$exe = "$root\build\examples\01_perso2d\$Config\perso2d.exe"
Write-Host "==> Binaire : $exe" -ForegroundColor Green

if ($Run) {
    if (Test-Path $exe) { & $exe } else { throw "Binaire introuvable : $exe" }
}
