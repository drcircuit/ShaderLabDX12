# complexity_report.ps1
# Thin PowerShell wrapper around tools/generate_complexity_report.py.
#
# Usage:
#   # Generate a fresh report from the default src/ directory
#   .\tools\complexity_report.ps1
#
#   # Specify a custom source root and output directory
#   .\tools\complexity_report.ps1 -Src src -Out reports
#
#   # Compare two previously saved report_data.json snapshots
#   .\tools\complexity_report.ps1 -Compare reports\report_data.json reports\report_data_new.json

[CmdletBinding()]
param(
    [string]$Src     = "src",
    [string]$Out     = "reports",
    [string]$Project = "ShaderLabDX12",
    [string[]]$Compare,    # Two paths: baseline.json current.json
    [string]$Title
)

$ErrorActionPreference = "Stop"
$scriptDir  = $PSScriptRoot
$repoRoot   = (Resolve-Path (Join-Path $scriptDir "..")).Path
$pythonScript = Join-Path $scriptDir "generate_complexity_report.py"

# Resolve Python executable
$pyExe = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $pyExe) {
    $pyExe = Get-Command python -ErrorAction SilentlyContinue
}
if (-not $pyExe) {
    Write-Error "Python 3 not found. Please install Python 3.8+ and add it to PATH."
    exit 1
}
$py = $pyExe.Source

Push-Location $repoRoot
try {
    if ($Compare -and $Compare.Length -eq 2) {
        $args_list = @($pythonScript, "--compare", $Compare[0], $Compare[1], "--out", $Out, "--project", $Project)
        if ($Title) { $args_list += @("--title", $Title) }
        Write-Host "Generating delta report..." -ForegroundColor Cyan
        & $py @args_list
    } else {
        $args_list = @($pythonScript, "--src", $Src, "--out", $Out, "--project", $Project)
        if ($Title) { $args_list += @("--title", $Title) }
        Write-Host "Generating complexity report for: $Src" -ForegroundColor Cyan
        & $py @args_list
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Report generation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    Write-Host ""
    Write-Host "Report written to: $Out" -ForegroundColor Green
    $htmlReport = if ($Compare) { Join-Path $Out "report_delta.html" } else { Join-Path $Out "report.html" }
    if (Test-Path $htmlReport) {
        Write-Host "Open: $htmlReport" -ForegroundColor Cyan
    }
} finally {
    Pop-Location
}
