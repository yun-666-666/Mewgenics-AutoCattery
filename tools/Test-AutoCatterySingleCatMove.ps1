[CmdletBinding(DefaultParameterSetName = 'List')]
param(
    [Parameter(Mandatory, ParameterSetName = 'List')]
    [switch]$ListPlacements,
    [Parameter(Mandatory, ParameterSetName = 'Move')]
    [switch]$MoveOne,
    [Parameter(Mandatory)]
    [string]$TestRoot,
    [Parameter(Mandatory)]
    [string]$TestSave,
    [string]$GameExecutable = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'Mewgenics.exe'),
    [Parameter(ParameterSetName = 'Move')]
    [string]$SqliteRuntime,
    [Parameter(Mandatory, ParameterSetName = 'Move')]
    [string]$OperationId,
    [Parameter(Mandatory, ParameterSetName = 'Move')]
    [ValidateRange(1, [int]::MaxValue)]
    [int]$CatIndex,
    [Parameter(Mandatory, ParameterSetName = 'Move')]
    [ValidateRange(1, [int]::MaxValue)]
    [int]$PlacementSourceIndex,
    [Parameter(Mandatory, ParameterSetName = 'Move')]
    [switch]$EnableCurrentBuildTestCopyWrite
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$labExe = Join-Path $projectRoot 'dist\Release\AutoCatterySaveLab.exe'
if (-not (Test-Path -LiteralPath $labExe)) {
    throw 'AutoCatterySaveLab.exe was not built. Run tools/build.ps1 -Configuration Release first.'
}

$arguments = @(
    '--test-root', $TestRoot,
    '--test-save', $TestSave,
    '--game-executable', $GameExecutable
)
if ($ListPlacements) {
    $arguments = @('--list-placements') + $arguments
} else {
    if (-not $SqliteRuntime) {
        $pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
        if (-not $pythonLauncher) {
            throw 'A SQLite 3.37.0+ runtime is required. Pass -SqliteRuntime with an absolute sqlite3.dll path.'
        }
        $SqliteRuntime = (& $pythonLauncher.Source -3 -c "import pathlib, _sqlite3; print(pathlib.Path(_sqlite3.__file__).with_name('sqlite3.dll'))") | Select-Object -First 1
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $SqliteRuntime -PathType Leaf)) {
            throw 'Python did not provide a usable sqlite3.dll. Pass -SqliteRuntime explicitly.'
        }
    }
    $arguments = @('--move-one') + $arguments + @(
        '--operation-id', $OperationId,
        '--cat-index', $CatIndex,
        '--placement-source-index', $PlacementSourceIndex,
        '--sqlite-runtime', $SqliteRuntime,
        '--enable-current-build-test-copy-write'
    )
}
& $labExe @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Single-cat test command failed with exit code $LASTEXITCODE."
}
