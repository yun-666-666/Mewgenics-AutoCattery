[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot
)

$ErrorActionPreference = 'Stop'
$resolvedGameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$editor = Join-Path $resolvedGameRoot 'mods\AutoCattery\AutoCatterySettings.exe'
if (-not (Test-Path -LiteralPath $editor)) {
    throw "AutoCatterySettings.exe is not installed: $editor"
}
Start-Process -FilePath $editor -WorkingDirectory (Split-Path -Parent $editor)
