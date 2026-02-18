# Check if environment is ready to build

Write-Host "ShaderLab Build Environment Check" -ForegroundColor Cyan
Write-Host "===================================" -ForegroundColor Cyan
Write-Host ""

$allReady = $true
$docsReady = $true

# Check CMake
Write-Host "CMake: " -NoNewline
if (Get-Command cmake -ErrorAction SilentlyContinue) {
    Write-Host "Installed" -ForegroundColor Green
} else {
    Write-Host "NOT FOUND" -ForegroundColor Red
    $allReady = $false
}

# Check Ninja
Write-Host "Ninja: " -NoNewline
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    Write-Host "Installed" -ForegroundColor Green
} else {
    Write-Host "NOT FOUND" -ForegroundColor Red
    $allReady = $false
}

# Check Visual Studio
Write-Host "Visual Studio 2022: " -NoNewline
$vsPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
)

$vsFound = $false
foreach ($path in $vsPaths) {
    $vcvars = Join-Path $path "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvars) {
        Write-Host "Installed at $path" -ForegroundColor Green
        $vsFound = $true
        break
    }
}

if (-not $vsFound) {
    Write-Host "NOT FOUND or STILL INSTALLING" -ForegroundColor Yellow
    Write-Host "  Visual Studio may still be installing..." -ForegroundColor Yellow
    $allReady = $false
}

# Check Dependencies
Write-Host ""
Write-Host "Dependencies:" -ForegroundColor Cyan

$deps = @{
    "Dear ImGui" = "third_party\imgui\imgui.h"
    "miniaudio" = "third_party\miniaudio\miniaudio.h"
    "nlohmann/json" = "third_party\json\include\nlohmann\json.hpp"
    "stb_image" = "third_party\stb\stb_image.h"
}

foreach ($dep in $deps.GetEnumerator()) {
    Write-Host "  $($dep.Key): " -NoNewline
    if (Test-Path $dep.Value) {
        Write-Host "OK" -ForegroundColor Green
    } else {
        Write-Host "MISSING" -ForegroundColor Red
        $allReady = $false
    }
}

Write-Host ""
Write-Host "Documentation:" -ForegroundColor Cyan
$docsCheckScript = Join-Path $PSScriptRoot "check_docs.ps1"
if (Test-Path $docsCheckScript) {
    & $docsCheckScript
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Docs check: OK" -ForegroundColor Green
    } else {
        Write-Host "  Docs check: FAILED" -ForegroundColor Red
        $docsReady = $false
    }
} else {
    Write-Host "  Docs check script missing" -ForegroundColor Yellow
    $docsReady = $false
}

Write-Host ""
Write-Host "===================================" -ForegroundColor Cyan

if ($allReady -and $docsReady) {
    Write-Host "Ready to build!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run: .\tools\build.ps1" -ForegroundColor Cyan
} else {
    Write-Host "Some checks failed" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "If Visual Studio is installing, wait for it to complete," -ForegroundColor White
    Write-Host "then restart your terminal and run this check again." -ForegroundColor White
}

Write-Host ""
