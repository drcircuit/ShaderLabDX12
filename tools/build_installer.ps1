param(
    [string]$AppName = "ShaderLab",
    [string]$BuildBin = "",
    [string]$OutputDir = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildBin)) {
    $BuildBin = Join-Path $repoRoot "build\bin"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "artifacts"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}

if (-not (Test-Path $BuildBin)) {
    throw "Build output folder not found: $BuildBin"
}

$editorExe = Join-Path $BuildBin "ShaderLabEditor.exe"
if (-not (Test-Path $editorExe)) {
    throw "Required runtime not found: $editorExe"
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmakePath = Join-Path $repoRoot "CMakeLists.txt"
    if (Test-Path $cmakePath) {
        $line = Select-String -Path $cmakePath -Pattern "project\(ShaderLab\s+VERSION\s+([0-9\.]+)" -AllMatches | Select-Object -First 1
        if ($line -and $line.Matches.Count -gt 0) {
            $Version = $line.Matches[0].Groups[1].Value
        }
    }
    if ([string]::IsNullOrWhiteSpace($Version)) {
        $Version = "0.1.0"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$stageRoot = Join-Path $repoRoot "build\installer_stage"
$stageApp = Join-Path $stageRoot "app"
$stagePrereq = Join-Path $stageRoot "prereqs"

New-Item -ItemType Directory -Force -Path $stageApp | Out-Null
New-Item -ItemType Directory -Force -Path $stagePrereq | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Get-ChildItem -Path $stageRoot -Force | Remove-Item -Recurse -Force
New-Item -ItemType Directory -Force -Path $stageApp | Out-Null
New-Item -ItemType Directory -Force -Path $stagePrereq | Out-Null

Write-Host "Staging application files from $BuildBin" -ForegroundColor Cyan
Copy-Item -Path (Join-Path $BuildBin "*") -Destination $stageApp -Recurse -Force

$extraToCopy = @(
    "templates",
    "editor_assets",
    "docs\\enduser",
    "LICENSE-COMMUNITY.md",
    "LICENSE-COMMERCIAL.md",
    "README.md"
)
foreach ($item in $extraToCopy) {
    $src = Join-Path $repoRoot $item
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $stageApp -Recurse -Force
    }
}

$openFontIconsSrc = Join-Path $repoRoot "third_party\OpenFontIcons"
if (Test-Path $openFontIconsSrc) {
    $thirdPartyDest = Join-Path $stageApp "third_party"
    New-Item -ItemType Directory -Force -Path $thirdPartyDest | Out-Null
    Copy-Item -Path $openFontIconsSrc -Destination $thirdPartyDest -Recurse -Force
}

function Resolve-VcRedist {
    if ($env:VCToolsRedistDir) {
        $candidate = Join-Path $env:VCToolsRedistDir "vc_redist.x64.exe"
        if (Test-Path $candidate) { return $candidate }
    }

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $installPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installPath)) {
            $redistRoot = Join-Path $installPath "VC\Redist\MSVC"
            if (Test-Path $redistRoot) {
                $latest = Get-ChildItem -Path $redistRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
                if ($latest) {
                    $candidate = Join-Path $latest.FullName "vc_redist.x64.exe"
                    if (Test-Path $candidate) { return $candidate }
                }
            }
        }
    }

    $fallbackRoots = @(
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC"
    )
    foreach ($root in $fallbackRoots) {
        if (-not (Test-Path $root)) { continue }
        $latest = Get-ChildItem -Path $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($latest) {
            $candidate = Join-Path $latest.FullName "vc_redist.x64.exe"
            if (Test-Path $candidate) { return $candidate }
        }
    }

    return $null
}

$vcRedistSource = Resolve-VcRedist
$vcRedistStaged = $null
if ($vcRedistSource) {
    $vcRedistStaged = Join-Path $stagePrereq "vc_redist.x64.exe"
    Copy-Item -Path $vcRedistSource -Destination $vcRedistStaged -Force
    Write-Host "Bundled VC++ runtime: $vcRedistSource" -ForegroundColor Green
} else {
    Write-Host "VC++ redistributable not found. Installer will be generated without bundled vc_redist.x64.exe." -ForegroundColor Yellow
}

function Resolve-Iscc {
    $command = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

$iscc = Resolve-Iscc

$issPath = Join-Path $stageRoot "shaderlab_installer.iss"

function Escape-InnoPath([string]$value) {
    return $value.Replace("\", "\\")
}

$stageAppEsc = Escape-InnoPath $stageApp
$outputDirEsc = Escape-InnoPath $OutputDir
$repoRootEsc = Escape-InnoPath $repoRoot

$issLines = @(
    ('#define MyAppName "{0}"' -f $AppName),
    ('#define MyAppVersion "{0}"' -f $Version),
    '#define MyAppPublisher "ShaderLab"',
    '#define MyAppExeName "ShaderLabEditor.exe"',
    ('#define SourceRoot "{0}"' -f $stageAppEsc),
    '',
    '[Setup]',
    'AppId={{A3FA0150-A7EA-4A72-B7EA-6AE309D375D5}',
    'AppName={#MyAppName}',
    'AppVersion={#MyAppVersion}',
    'AppPublisher={#MyAppPublisher}',
    'DefaultDirName={autopf}\{#MyAppName}',
    'DefaultGroupName={#MyAppName}',
    'DisableProgramGroupPage=yes',
    ('OutputBaseFilename=ShaderLabSetup-x64-{0}-{1}' -f $Version, $timestamp),
    ('OutputDir={0}' -f $outputDirEsc),
    'Compression=lzma2',
    'SolidCompression=yes',
    'ArchitecturesAllowed=x64compatible',
    'ArchitecturesInstallIn64BitMode=x64compatible',
    'SetupIconFile={#SourceRoot}\\editor_assets\\shaderlab.ico.ico',
    ('LicenseFile={0}\LICENSE-COMMUNITY.md' -f $repoRootEsc),
    '',
    '[Languages]',
    'Name: "english"; MessagesFile: "compiler:Default.isl"',
    '',
    '[Tasks]',
    'Name: "desktopicon"; Description: "Create a desktop icon"; GroupDescription: "Additional icons:"',
    '',
    '[Files]',
    'Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs'
)

if ($vcRedistStaged) {
    $vcRedistEsc = Escape-InnoPath $vcRedistStaged
    $issLines += ('Source: "{0}"; DestDir: "{{tmp}}"; Flags: deleteafterinstall' -f $vcRedistEsc)
}

$issLines += @(
    '',
    '[Icons]',
    'Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\editor_assets\shaderlab.ico.ico"; AppUserModelID: "ShaderLab.Editor"',
    'Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\editor_assets\shaderlab.ico.ico"; AppUserModelID: "ShaderLab.Editor"',
    '',
    '[Run]'
)

if ($vcRedistStaged) {
    $issLines += 'Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ Runtime..."; Flags: runhidden waituntilterminated'
}

$issLines += 'Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent'

Set-Content -Path $issPath -Value $issLines -Encoding UTF8

if ($iscc) {
    Write-Host "Building installer with Inno Setup: $iscc" -ForegroundColor Cyan
    & $iscc $issPath
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup build failed with exit code $LASTEXITCODE"
    }
    Write-Host "Installer created in: $OutputDir" -ForegroundColor Green
    exit 0
}

Write-Host "Inno Setup (ISCC.exe) not found. Creating fallback portable artifact." -ForegroundColor Yellow
$zipName = "ShaderLabSetup-x64-$Version-$timestamp-portable.zip"
$zipPath = Join-Path $OutputDir $zipName
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}
Compress-Archive -Path (Join-Path $stageApp "*") -DestinationPath $zipPath -Force
Write-Host "Portable artifact created: $zipPath" -ForegroundColor Green
