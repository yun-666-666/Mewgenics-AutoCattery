[CmdletBinding(DefaultParameterSetName = 'List')]
param(
    [Parameter(Mandatory, ParameterSetName = 'List')]
    [switch]$List,
    [Parameter(Mandatory, ParameterSetName = 'Verify')]
    [switch]$VerifyOnly,
    [Parameter(Mandatory, ParameterSetName = 'Restore')]
    [switch]$Restore,
    [Parameter(Mandatory, ParameterSetName = 'Verify')]
    [Parameter(Mandatory, ParameterSetName = 'Restore')]
    [string]$OperationId,
    [Parameter(Mandatory, ParameterSetName = 'Restore')]
    [string]$RestoreOperationId,
    [Parameter(Mandatory, ParameterSetName = 'Restore')]
    [string]$TargetSave,
    [Parameter(Mandatory, ParameterSetName = 'Restore')]
    [string]$GameExecutable,
    [string]$BackupRoot = (Join-Path $env:APPDATA 'AutoCatteryData\backups')
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$restoreExe = Join-Path $projectRoot 'dist\Release\AutoCatteryRestore.exe'
if (-not (Test-Path -LiteralPath $restoreExe)) {
    throw 'AutoCatteryRestore.exe was not built. Run tools/build.ps1 -Configuration Release first.'
}

$arguments = @('--backup-root', $BackupRoot)
if ($List) {
    $arguments += '--list'
} elseif ($VerifyOnly) {
    $arguments += @('--verify-only', '--operation-id', $OperationId)
} else {
    $arguments += @(
        '--restore',
        '--operation-id', $OperationId,
        '--restore-operation-id', $RestoreOperationId,
        '--target-save', $TargetSave,
        '--game-executable', $GameExecutable
    )
}
& $restoreExe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Restore command failed with exit code $LASTEXITCODE."
}
