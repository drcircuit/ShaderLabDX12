param(
    [string]$ProjectPath = "",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cli = Join-Path $repo "build\bin\ShaderLabBuildCli.exe"

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $candidates = @(
        (Join-Path $repo "DEMO.json"),
        (Join-Path (Split-Path $repo -Parent) "DEMO.json")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $ProjectPath = (Resolve-Path $candidate).Path
            break
        }
    }
}

if (-not (Test-Path $cli)) {
    throw "ShaderLabBuildCli missing: $cli"
}
if (-not (Test-Path $ProjectPath)) {
    throw "Project JSON missing: $ProjectPath. Pass -ProjectPath or add DEMO.json in repo root/parent."
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repo "build_smoke_crinkled"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-Crinkled {
    param(
        [string]$Name,
        [string]$Target,
        [string]$Ext,
        [string]$Size = "none"
    )

    $root = Join-Path $OutputRoot ("root_" + $Name)
    $out = Join-Path $OutputRoot ("artifact_" + $Name + $Ext)

    Write-Host "--- $Name (crinkled) ---" -ForegroundColor Cyan
    & $cli --app-root $repo --project $ProjectPath --output $out --solution-root $root --target $Target --mode crinkled --size $Size

    $code = $LASTEXITCODE
    $exists = Test-Path $out
    $sizeBytes = if ($exists) { (Get-Item $out).Length } else { -1 }

    [PSCustomObject]@{
        Name = $Name
        Target = $Target
        ExitCode = $code
        Exists = $exists
        SizeBytes = $sizeBytes
        Output = $out
    }
}

$results = @()
$results += Invoke-Crinkled -Name "packaged" -Target "packaged" -Ext ".zip"
$results += Invoke-Crinkled -Name "selfcontained" -Target "selfcontained" -Ext ".exe"
$results += Invoke-Crinkled -Name "screensaver" -Target "screensaver" -Ext ".scr"
$results += Invoke-Crinkled -Name "micro" -Target "micro" -Ext ".exe" -Size "64k"

Write-Host "--- SUMMARY ---" -ForegroundColor Yellow
$results | Format-Table -AutoSize

$failures = $results | Where-Object { -not ($_.ExitCode -eq 0 -and $_.Exists) }
if ($failures.Count -eq 0) {
    Write-Host "ALL_CRINKLED_OK" -ForegroundColor Green
    exit 0
}

Write-Host "CRINKLED_MATRIX_HAS_FAILURES" -ForegroundColor Red
exit 1
