[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$GameRoot,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$resolvedGameRoot = (Resolve-Path -LiteralPath $GameRoot).Path

if (-not (Test-Path -LiteralPath (Join-Path $resolvedGameRoot 'Mewgenics.exe'))) {
    throw 'GameRoot does not contain Mewgenics.exe.'
}
if (-not (Test-Path -LiteralPath (Join-Path $resolvedGameRoot 'version.dll'))) {
    throw 'Mewjector version.dll is missing.'
}

$source = Join-Path $projectRoot "dist\$Configuration"
$dllSource = Join-Path $source 'AutoCattery.dll'
if (-not (Test-Path -LiteralPath $dllSource)) {
    throw "Build output is missing: $dllSource"
}

$mods = Join-Path $resolvedGameRoot 'mods'
$runtimeRoot = Join-Path $mods 'AutoCattery'
$mewtatorConfigPath = Join-Path $resolvedGameRoot 'Mewtator\config.json'
if (-not (Test-Path -LiteralPath $mewtatorConfigPath)) {
    throw 'Mewtator config.json is missing; AutoCattery UI assets require a data-mod loader.'
}

$mewtatorConfig = Get-Content -LiteralPath $mewtatorConfigPath -Raw | ConvertFrom-Json
if (-not $mewtatorConfig.mod_folder) {
    throw 'Mewtator config.json does not define mod_folder.'
}

$mewtatorMods = [System.IO.Path]::GetFullPath([string]$mewtatorConfig.mod_folder)
$dataModRoot = Join-Path $mewtatorMods 'AutoCattery'
$modListPath = Join-Path $mewtatorMods 'modlist.txt'
if ($PSCmdlet.ShouldProcess($resolvedGameRoot, 'Deploy AutoCattery files')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $runtimeRoot 'config') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $runtimeRoot 'localization') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $dataModRoot 'data\text') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $dataModRoot 'swfs') | Out-Null
    Copy-Item -LiteralPath $dllSource -Destination (Join-Path $mods 'AutoCattery.dll') -Force
    Copy-Item -LiteralPath (Join-Path $source 'AutoCatterySettings.exe') -Destination $runtimeRoot -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\default_config.json') -Destination (Join-Path $runtimeRoot 'config') -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\config.schema.json') -Destination (Join-Path $runtimeRoot 'config') -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\scene_signatures.json') -Destination (Join-Path $runtimeRoot 'config') -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\protection.schema.json') -Destination (Join-Path $runtimeRoot 'config') -Force
    $protectionPath = Join-Path $runtimeRoot 'config\protection.json'
    if (-not (Test-Path -LiteralPath $protectionPath)) {
        Copy-Item -LiteralPath (Join-Path $source 'config\protection.json') -Destination $protectionPath
    }
    Copy-Item -LiteralPath (Join-Path $source 'localization\strings.json') -Destination (Join-Path $runtimeRoot 'localization') -Force
    Copy-Item -LiteralPath (Join-Path $source 'THIRD_PARTY_NOTICES.md') -Destination $runtimeRoot -Force

    Copy-Item -LiteralPath (Join-Path $source 'data\text\combined.csv.append') -Destination (Join-Path $dataModRoot 'data\text') -Force
    Copy-Item -LiteralPath (Join-Path $source 'swfs\auto_cattery_house.swf') -Destination (Join-Path $dataModRoot 'swfs') -Force
    Copy-Item -LiteralPath (Join-Path $source 'swfs\swflist.gon.append') -Destination (Join-Path $dataModRoot 'swfs') -Force
    Copy-Item -LiteralPath (Join-Path $source 'description.json') -Destination $dataModRoot -Force

    $enabledMods = @(if (Test-Path -LiteralPath $modListPath) {
        @(Get-Content -LiteralPath $modListPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    } else {
        @()
    })
    if ($enabledMods -notcontains 'AutoCattery') {
        $enabledMods += 'AutoCattery'
        Set-Content -LiteralPath $modListPath -Value $enabledMods -Encoding utf8
    }
}

Write-Host 'DLL deployed to the non-recursive Mewjector mods directory.'
Write-Host "UI data mod deployed and enabled for Mewtator: $dataModRoot"
Write-Host 'Launch the game through Mewtator so its enabled data-mod paths are passed to Mewgenics.'
