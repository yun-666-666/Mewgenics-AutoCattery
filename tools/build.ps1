[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vsRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$dumpbin = Join-Path $vsRoot 'VC\Tools\MSVC'

if (-not (Test-Path -LiteralPath $cmake)) {
    throw 'Visual Studio bundled CMake was not found.'
}

$buildDirectory = Join-Path $projectRoot 'build'
& $cmake -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

& $cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

& $cmake --build $buildDirectory --config $Configuration --target RUN_TESTS
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

$dist = Join-Path $projectRoot "dist\$Configuration"
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'config') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'data\text') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'localization') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'swfs') | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory "out\$Configuration\AutoCattery.dll") -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $buildDirectory "out\$Configuration\AutoCatterySettings.exe") -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\default_config.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\config.schema.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\scene_signatures.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\protection.schema.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\data\text\combined.csv.append') -Destination (Join-Path $dist 'data\text') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\localization\strings.json') -Destination (Join-Path $dist 'localization') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\swfs\auto_cattery_house.swf') -Destination (Join-Path $dist 'swfs') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\swfs\swflist.gon.append') -Destination (Join-Path $dist 'swfs') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\description.json') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $dist -Force

$dumpbinExe = Get-ChildItem -LiteralPath $dumpbin -Filter dumpbin.exe -Recurse |
    Where-Object FullName -Match '\\bin\\Hostx64\\x64\\dumpbin.exe$' |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $dumpbinExe) { throw 'x64 dumpbin.exe was not found.' }

$dll = Join-Path $dist 'AutoCattery.dll'
$exports = (& $dumpbinExe /exports $dll) -join "`n"
if ($exports -notmatch 'AutoCattery_Initialize' -or $exports -notmatch 'AutoCattery_Shutdown') {
    throw 'Required DLL exports are missing.'
}
$headers = (& $dumpbinExe /headers $dll) -join "`n"
if ($headers -notmatch 'machine \(x64\)') {
    throw 'Built DLL is not x64.'
}

Write-Host "Built and verified: $dll"
