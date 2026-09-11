[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [switch]$SkipBuild,

    [switch]$SkipSign,

    [string]$CertificateThumbprint,

    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$installerSource = Join-Path $repoRoot "installer\Product.wxs"

function Find-SignTool {
    $command = Get-Command signtool -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (Test-Path $sdkRoot) {
        $found = Get-ChildItem "$sdkRoot\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
            Sort-Object { [version]$_.Directory.Parent.Name } -Descending |
            Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    }

    throw "signtool.exe was not found. Install the Windows SDK or pass -SkipSign."
}

function Get-ProjectVersion {
    $cmakeLists = Get-Content (Join-Path $repoRoot "CMakeLists.txt") -Raw
    if ($cmakeLists -notmatch 'project\s*\(\s*srt-dubber\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        throw "Could not read the srt-dubber version from CMakeLists.txt."
    }

    return $Matches[1]
}

function Invoke-SignAndVerify {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,

        [Parameter(Mandatory)]
        [string]$SignTool
    )

    $selector = @("/a")
    if (-not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        $selector = @("/sha1", ($CertificateThumbprint -replace '\s', ''))
    }

    Write-Host "Signing: $FilePath"
    $signArguments = @(
        "sign"
        "/v"
        "/fd", "SHA256"
        "/tr", $TimestampUrl
        "/td", "SHA256"
    ) + $selector + @($FilePath)
    & $SignTool @signArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Signing failed for $FilePath with exit code $LASTEXITCODE."
    }

    Write-Host "Verifying: $FilePath"
    & $SignTool verify /pa /v $FilePath
    if ($LASTEXITCODE -ne 0) {
        throw "Signature verification failed for $FilePath with exit code $LASTEXITCODE."
    }
}

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot "build-windows.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            throw "Windows build failed with exit code $LASTEXITCODE."
        }
    }

    $configurationDir = Join-Path $buildDir $Configuration
    $exePath = Join-Path $configurationDir "srt-dubber.exe"
    if (-not (Test-Path $exePath)) {
        throw "srt-dubber.exe was not found at $exePath. Build it first or omit -SkipBuild."
    }

    $productVersion = Get-ProjectVersion
    $msiPath = Join-Path $buildDir "srt-dubber-$productVersion-windows-x64.msi"

    $signTool = $null
    if (-not $SkipSign) {
        $signTool = Find-SignTool
        Invoke-SignAndVerify -FilePath $exePath -SignTool $signTool
    }

    $wix = Get-Command wix -ErrorAction SilentlyContinue
    if (-not $wix) {
        throw "WiX CLI was not found. Install WiX 5 with: dotnet tool install --global wix --version 5.0.1"
    }

    Write-Host "Building MSI: $msiPath"
    $wixArguments = @(
        "build"
        $installerSource
        "-arch", "x64"
        "-d", "BuildDir=$configurationDir"
        "-d", "RepoRoot=$repoRoot"
        "-d", "ProductVersion=$productVersion"
        "-ext", "WixToolset.UI.wixext"
        "-pdbtype", "none"
        "-o", $msiPath
    )
    & $wix.Source @wixArguments
    if ($LASTEXITCODE -ne 0) {
        throw "WiX build failed with exit code $LASTEXITCODE."
    }

    if (-not $SkipSign) {
        Invoke-SignAndVerify -FilePath $msiPath -SignTool $signTool
    }

    Write-Host "Packaged: $msiPath"
    if ($SkipSign) {
        Write-Host "Signing was skipped."
    }
}
finally {
    Pop-Location
}