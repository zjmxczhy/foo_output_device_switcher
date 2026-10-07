$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$VcRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build"
$Proj = Join-Path $Root "third_party\foobar2000-sdk\foobar2000\foo_output_device_switcher\foo_output_device_switcher.vcxproj"
$TolkSource = Join-Path $Root "third_party\tolk-isolated\src"
$TolkBuildRoot = Join-Path $Root "build\tolk-isolated"

$TolkCompileSources = @(
    "Tolk.cpp",
    "ScreenReaderDriverBOY.cpp",
    "ScreenReaderDriverJAWS.cpp",
    "ScreenReaderDriverNVDA.cpp",
    "ScreenReaderDriverSA.cpp",
    "ScreenReaderDriverSNova.cpp",
    "ScreenReaderDriverWE.cpp",
    "ScreenReaderDriverZDSR.cpp",
    "ScreenReaderDriverZT.cpp",
    "ScreenReaderDriverSAPI.cpp",
    "fsapi.c",
    "wineyes.c",
    "zt.c"
)

function Assert-NativeExitCode($Description) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Get-Vcvars($Name) {
    $Vcvars = Join-Path $VcRoot $Name
    if (-not (Test-Path $Vcvars)) { throw "Missing Visual Studio env script: $Vcvars" }
    return $Vcvars
}

function Build-Tolk($Architecture, $VcvarsName) {
    $Vcvars = Get-Vcvars $VcvarsName
    if (-not (Test-Path (Join-Path $TolkSource "Tolk.cpp"))) {
        throw "Missing isolated Tolk source directory: $TolkSource"
    }

    $TolkOut = Join-Path $TolkBuildRoot $Architecture
    New-Item -ItemType Directory -Force -Path $TolkOut | Out-Null
    Remove-Item -LiteralPath (Join-Path $TolkOut "Tolk.dll"), (Join-Path $TolkOut "Tolk.res") -Force -ErrorAction SilentlyContinue

    $SourceArguments = $TolkCompileSources -join " "
    $Command = "`"$Vcvars`" && cd /d `"$TolkSource`" && rc /nologo /fo `"$TolkOut\Tolk.res`" Tolk.rc && cl /nologo /O2 /EHsc /std:c++17 /utf-8 /LD /Gw /W4 /wd4127 /D_EXPORTING /DUNICODE /Fe:`"$TolkOut\Tolk.dll`" $SourceArguments `"$TolkOut\Tolk.res`" User32.Lib Ole32.Lib OleAut32.Lib"
    Write-Host "Building isolated Tolk $Architecture..."
    cmd /c $Command
    Assert-NativeExitCode "Tolk $Architecture build"
    if (-not (Test-Path (Join-Path $TolkOut "Tolk.dll"))) {
        throw "Tolk $Architecture build did not produce Tolk.dll."
    }
}

function Build-Component($Platform, $VcvarsName) {
    $Vcvars = Get-Vcvars $VcvarsName
    Write-Host "Building foo_output_device_switcher $Platform..."
    cmd /c "`"$Vcvars`" && msbuild `"$Proj`" /p:Configuration=Release /p:Platform=$Platform /p:PlatformToolset=v143 /m"
    Assert-NativeExitCode "foo_output_device_switcher $Platform build"
}

Build-Tolk "x64" "vcvars64.bat"
Build-Component "x64" "vcvars64.bat"
Build-Tolk "x86" "vcvars32.bat"
Build-Component "Win32" "vcvars32.bat"

& (Join-Path $Root "package_output_device_switcher.ps1")
