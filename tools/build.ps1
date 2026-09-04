[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vsRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$dumpbin = Join-Path $vsRoot 'VC\Tools\MSVC'
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$ninja = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'

if (-not (Test-Path -LiteralPath $cmake)) {
    throw 'Visual Studio bundled CMake was not found.'
}

$buildDirectory = Join-Path $projectRoot 'build'
$registeredVisualStudio = if (Test-Path -LiteralPath $vswhere) {
    & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
} else {
    $null
}
if ($registeredVisualStudio) {
    & $cmake -S $projectRoot -B $buildDirectory -G 'Visual Studio 17 2022' -A x64
} else {
    if (-not (Test-Path -LiteralPath $vcvars) -or
        -not (Test-Path -LiteralPath $ninja)) {
        throw 'Neither a registered Visual Studio instance nor the bundled MSVC/Ninja toolchain is available.'
    }
    $buildDirectory = Join-Path $projectRoot 'build-ninja'
    $environment = & cmd.exe /d /s /c "`"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($line in $environment) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable(
                $line.Substring(0, $separator),
                $line.Substring($separator + 1),
                'Process')
        }
    }
    & $cmake -S $projectRoot -B $buildDirectory -G 'Ninja Multi-Config' `
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=$ninja"
}
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

& $cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

& $ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

$dist = Join-Path $projectRoot "dist\$Configuration"
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'config') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'data\text') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'data\classes') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'localization') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dist 'swfs') | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory "out\$Configuration\AutoCattery.dll") -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\default_config.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\config.schema.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\scene_signatures.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\protection.schema.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config\protection.json') -Destination (Join-Path $dist 'config') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\data\text\combined.csv.append') -Destination (Join-Path $dist 'data\text') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\data\classes\classes.gon.merge') -Destination (Join-Path $dist 'data\classes') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\data\classes\advanced_classes.gon.merge') -Destination (Join-Path $dist 'data\classes') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\localization\strings.json') -Destination (Join-Path $dist 'localization') -Force
Copy-Item -LiteralPath (Join-Path $buildDirectory 'generated\auto_cattery_house.swf') -Destination (Join-Path $dist 'swfs') -Force
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
