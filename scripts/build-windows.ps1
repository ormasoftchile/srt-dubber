[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

function Find-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $standalone = Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"
    if (Test-Path $standalone) {
        return $standalone
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
        if ($installPath) {
            $bundled = Join-Path $installPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $bundled) {
                return $bundled
            }
        }
    }

    $visualStudioRoots = @(
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022")
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022")
    )
    foreach ($root in $visualStudioRoots) {
        if (-not (Test-Path $root)) {
            continue
        }

        foreach ($installation in Get-ChildItem -Path $root -Directory) {
            $bundled = Join-Path $installation.FullName "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $bundled) {
                return $bundled
            }
        }
    }

    throw "CMake was not found. Add the CMake component to Visual Studio or install Kitware.CMake with winget."
}

function Add-GitToolsToPath {
    if (Get-Command patch -ErrorAction SilentlyContinue) {
        return
    }

    $gitTools = Join-Path $env:ProgramFiles "Git\usr\bin"
    if (Test-Path (Join-Path $gitTools "patch.exe")) {
        $env:Path += ";$gitTools"
        return
    }

    throw "patch.exe was not found. Install Git for Windows before building."
}

Push-Location $repoRoot
try {
    $cmake = Find-CMake
    Add-GitToolsToPath

    $miniaudio = Join-Path $repoRoot "vendor\miniaudio.h"
    if (-not (Test-Path $miniaudio)) {
        Write-Host "Downloading miniaudio 0.11.21..."
        Invoke-WebRequest `
            -Uri "https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h" `
            -OutFile $miniaudio
    }

    & $cmake -S $repoRoot -B $buildDir
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    & $cmake --build $buildDir --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    $binary = Join-Path $buildDir "$Configuration\srt-dubber.exe"
    if (-not (Test-Path $binary)) {
        $binary = Join-Path $buildDir "srt-dubber.exe"
    }

    Write-Host "Built: $binary"
}
finally {
    Pop-Location
}