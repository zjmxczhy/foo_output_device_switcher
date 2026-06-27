$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$VcRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build"
$Proj = Join-Path $Root "third_party\foobar2000-sdk\foobar2000\foo_output_device_switcher\foo_output_device_switcher.vcxproj"

function Get-Vcvars($Name) {
    $Vcvars = Join-Path $VcRoot $Name
    if (-not (Test-Path $Vcvars)) { throw "Missing Visual Studio env script: $Vcvars" }
    return $Vcvars
}

function Build-Component($Platform, $VcvarsName) {
    $Vcvars = Get-Vcvars $VcvarsName
    Write-Host "Building foo_output_device_switcher $Platform..."
    cmd /c "`"$Vcvars`" && msbuild `"$Proj`" /p:Configuration=Release /p:Platform=$Platform /p:PlatformToolset=v143 /m"
}

Build-Component "x64" "vcvars64.bat"
Build-Component "Win32" "vcvars32.bat"

& (Join-Path $Root "package_output_device_switcher.ps1")

