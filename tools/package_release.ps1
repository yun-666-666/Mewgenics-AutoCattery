[CmdletBinding()]
param(
    [string]$Version = '0.5.4',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $projectRoot "dist\$Configuration"
$releaseRoot = Join-Path $projectRoot 'dist\releases'
$packageName = "AutoCattery-v$Version-Windows-x64"
$workRoot = Join-Path $releaseRoot ".package-$Version"
$packageRoot = Join-Path $workRoot $packageName
$sourceRoot = Join-Path $workRoot "AutoCattery-v$Version-source"

if (-not (Test-Path -LiteralPath (Join-Path $distRoot 'AutoCattery.dll'))) {
    throw "Missing $Configuration build. Run tools\build.ps1 first."
}

New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
$releaseResolved = [System.IO.Path]::GetFullPath($releaseRoot)
$workResolved = [System.IO.Path]::GetFullPath($workRoot)
if (-not $workResolved.StartsWith($releaseResolved + [System.IO.Path]::DirectorySeparatorChar)) {
    throw 'Refusing to clean a packaging path outside dist\releases.'
}
if (Test-Path -LiteralPath $workRoot) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force
}

$runtimeRoot = Join-Path $packageRoot 'Mewjector\mods\AutoCattery'
$dataRoot = Join-Path $packageRoot 'Mewtator\AutoCattery'
$docsRoot = Join-Path $packageRoot 'Documentation'
New-Item -ItemType Directory -Force -Path $runtimeRoot, $dataRoot, $docsRoot | Out-Null

Copy-Item -LiteralPath (Join-Path $distRoot 'AutoCattery.dll') -Destination (Split-Path $runtimeRoot -Parent)
Copy-Item -LiteralPath (Join-Path $distRoot 'AutoCatterySettings.exe') -Destination $runtimeRoot
Copy-Item -LiteralPath (Join-Path $distRoot 'config') -Destination $runtimeRoot -Recurse
Copy-Item -LiteralPath (Join-Path $distRoot 'localization') -Destination $runtimeRoot -Recurse
Copy-Item -LiteralPath (Join-Path $distRoot 'data') -Destination $dataRoot -Recurse
Copy-Item -LiteralPath (Join-Path $distRoot 'swfs') -Destination $dataRoot -Recurse
Copy-Item -LiteralPath (Join-Path $distRoot 'description.json') -Destination $dataRoot

$documentation = @(
    'README.md',
    'README_EN.md',
    'ACKNOWLEDGEMENTS.md',
    'CHANGELOG.md',
    'THIRD_PARTY_NOTICES.md',
    'docs\GAME_VALUE_REFERENCE.md',
    'docs\USER_GUIDE.md',
    "docs\RELEASE_NOTES_v$Version.md"
)
foreach ($document in $documentation) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $document) -Destination $docsRoot
}

$binaryZip = Join-Path $releaseRoot "$packageName.zip"
if (Test-Path -LiteralPath $binaryZip) { Remove-Item -LiteralPath $binaryZip -Force }
Compress-Archive -LiteralPath $packageRoot -DestinationPath $binaryZip -CompressionLevel Optimal

New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null
$mainArchive = Join-Path $workRoot 'main.tar'
& git -C $projectRoot archive --format=tar --output=$mainArchive HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not archive the main repository.' }
& tar -xf $mainArchive -C $sourceRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not extract the main source archive.' }

$submodulePaths = & git -C $projectRoot config -f .gitmodules --get-regexp path |
    ForEach-Object { ($_ -split '\s+', 2)[1] }
foreach ($submodulePath in $submodulePaths) {
    $submoduleRoot = Join-Path $projectRoot $submodulePath
    $targetRoot = Join-Path $sourceRoot $submodulePath
    $safeName = $submodulePath.Replace('/', '_').Replace('\', '_')
    $subArchive = Join-Path $workRoot "$safeName.tar"
    New-Item -ItemType Directory -Force -Path $targetRoot | Out-Null
    & git -C $submoduleRoot archive --format=tar --output=$subArchive HEAD
    if ($LASTEXITCODE -ne 0) { throw "Could not archive submodule: $submodulePath" }
    & tar -xf $subArchive -C $targetRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not extract submodule: $submodulePath" }
}

$sourceZip = Join-Path $releaseRoot "AutoCattery-v$Version-source.zip"
if (Test-Path -LiteralPath $sourceZip) { Remove-Item -LiteralPath $sourceZip -Force }
Compress-Archive -LiteralPath $sourceRoot -DestinationPath $sourceZip -CompressionLevel Optimal

Remove-Item -LiteralPath $workRoot -Recurse -Force
Get-FileHash -Algorithm SHA256 -LiteralPath $binaryZip, $sourceZip |
    Select-Object Path, Hash
