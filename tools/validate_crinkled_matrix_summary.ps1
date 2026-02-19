param(
    [string]$ProjectPath = "C:\Users\espen\code\DEMO.json",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cli = Join-Path $repo "build\bin\ShaderLabBuildCli.exe"
if (-not (Test-Path $cli)) { throw "ShaderLabBuildCli missing: $cli" }
if (-not (Test-Path $ProjectPath)) { throw "Project JSON missing: $ProjectPath" }

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repo "build_smoke_crinkled_summary"
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
    $log = Join-Path $OutputRoot ("log_" + $Name + ".txt")

    & $cli --app-root $repo --project $ProjectPath --output $out --solution-root $root --target $Target --mode crinkled --size $Size *> $log
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
        Log = $log
    }
}

$results = @()
$results += Invoke-Crinkled -Name "packaged" -Target "packaged" -Ext ".zip"
$results += Invoke-Crinkled -Name "selfcontained" -Target "selfcontained" -Ext ".exe"
$results += Invoke-Crinkled -Name "screensaver" -Target "screensaver" -Ext ".scr"
$results += Invoke-Crinkled -Name "micro" -Target "micro" -Ext ".exe" -Size "64k"

$results | Sort-Object Name | Format-Table -AutoSize

$failures = $results | Where-Object { -not ($_.ExitCode -eq 0 -and $_.Exists) }
if ($failures.Count -eq 0) {
    Write-Host "ALL_CRINKLED_OK" -ForegroundColor Green
    exit 0
}

Write-Host "CRINKLED_MATRIX_HAS_FAILURES" -ForegroundColor Red
foreach ($f in $failures) {
    Write-Host (" - " + $f.Name + " (log: " + $f.Log + ")") -ForegroundColor Yellow
}
exit 1
