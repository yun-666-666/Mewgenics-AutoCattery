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
$dataRoot = Join-Path $mods 'AutoCattery'
if ($PSCmdlet.ShouldProcess($resolvedGameRoot, 'Deploy AutoCattery files')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dataRoot 'config') | Out-Null
    Copy-Item -LiteralPath $dllSource -Destination (Join-Path $mods 'AutoCattery.dll') -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\default_config.json') -Destination (Join-Path $dataRoot 'config') -Force
    Copy-Item -LiteralPath (Join-Path $source 'config\scene_signatures.json') -Destination (Join-Path $dataRoot 'config') -Force
    Copy-Item -LiteralPath (Join-Path $source 'THIRD_PARTY_NOTICES.md') -Destination $dataRoot -Force
}

Write-Host 'Deployment keeps the DLL directly under mods because Mewjector scan is non-recursive.'
