$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$DistRoot = Join-Path $Root "dist"
$TolkSupportRoot = Join-Path $Root "third_party\tolk-with-zdsr"
$TolkBuildRoot = Join-Path $Root "build\tolk-isolated"
$TolkFileName = "foo_output_device_switcher_tolk.dll"
$MainSource = Get-Content (Join-Path $Root "third_party\foobar2000-sdk\foobar2000\foo_output_device_switcher\main.cpp") -Raw -Encoding UTF8
$VersionMatch = [regex]::Match($MainSource, 'DECLARE_COMPONENT_VERSION\([^,]+,\s*"([^"]+)"')
if (-not $VersionMatch.Success) { throw "Unable to read component version from main.cpp." }
$Version = $VersionMatch.Groups[1].Value

function Require-File($Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing file: $Path" }
}

function Require-Directory($Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { throw "Missing directory: $Path" }
}

function Contains-FixedByteSequence(
    [byte[]]$Data,
    [byte[]]$Pattern
) {
    if ($Pattern.Length -eq 0 -or $Data.Length -lt $Pattern.Length) {
        return $false
    }

    for ($Index = 0; $Index -le $Data.Length - $Pattern.Length; $Index++) {
        $Equal = $true
        for ($Offset = 0; $Offset -lt $Pattern.Length; $Offset++) {
            if ($Data[$Index + $Offset] -ne $Pattern[$Offset]) {
                $Equal = $false
                break
            }
        }
        if ($Equal) { return $true }
    }

    return $false
}

function Get-DriverMap($ArchName) {
    if ($ArchName -eq "x64") {
        return @{
            "nvdaControllerClient64.dll" = "ods-nvda-client-64bits.dll"
            "byctrl-x64.dll" = "ods-br-x64.dll"
            "ZDSRAPI_x64.dll" = "ods-zsr_x64.dll"
            "SAAPI64.dll" = "odssa64.dll"
        }
    } elseif ($ArchName -eq "x86") {
        return @{
            "nvdaControllerClient32.dll" = "ods-nvda-client-32bits.dll"
            "byctrl.dll" = "ods-br.dll"
            "ZDSRAPI.dll" = "ods-zsr.dll"
            "SAAPI32.dll" = "odssa32.dll"
            "dolapi32.dll" = "odsdol32.dll"
        }
    } else {
        throw "Unsupported Tolk architecture: $ArchName"
    }
}

function Get-PeMachine($Path) {
    $Data = [IO.File]::ReadAllBytes($Path)
    if ($Data.Length -lt 64 -or $Data[0] -ne 0x4d -or $Data[1] -ne 0x5a) {
        throw "Not a Windows PE file: $Path"
    }

    $PeOffset = [BitConverter]::ToInt32($Data, 0x3c)
    if ($PeOffset -lt 0 -or $PeOffset + 6 -gt $Data.Length) {
        throw "Invalid PE header: $Path"
    }
    if ($Data[$PeOffset] -ne 0x50 -or $Data[$PeOffset + 1] -ne 0x45 -or
        $Data[$PeOffset + 2] -ne 0 -or $Data[$PeOffset + 3] -ne 0) {
        throw "Invalid PE signature: $Path"
    }

    return [BitConverter]::ToUInt16($Data, $PeOffset + 4)
}

function Assert-PeArchitecture($Path, $ArchName) {
    $Expected = if ($ArchName -eq "x64") { 0x8664 } elseif ($ArchName -eq "x86") { 0x14c } else { throw "Unsupported Tolk architecture: $ArchName" }
    $Actual = Get-PeMachine $Path
    if ($Actual -ne $Expected) {
        throw "Wrong PE architecture for $Path. Expected $ArchName, machine 0x$('{0:X4}' -f $Expected), got 0x$('{0:X4}' -f $Actual)."
    }
}

function Assert-TolkDriverNames($Path, $ArchName) {
    $Data = [IO.File]::ReadAllBytes($Path)
    $DriverMap = Get-DriverMap $ArchName
    foreach ($OriginalName in $DriverMap.Keys) {
        $AsciiFound = Contains-FixedByteSequence $Data ([Text.Encoding]::ASCII.GetBytes($OriginalName))
        $UnicodeFound = Contains-FixedByteSequence $Data ([Text.Encoding]::Unicode.GetBytes($OriginalName))
        if ($AsciiFound -or $UnicodeFound) {
            throw "The isolated Tolk runtime still contains the original driver name: $OriginalName"
        }
    }
}

function Copy-Payload($TargetDir, $ArchName, $ComponentPlatform) {
    $ComponentDll = Join-Path $Root "build\foo_output_device_switcher\Release\$ComponentPlatform\foo_output_device_switcher.dll"
    $TolkBuild = Join-Path $TolkBuildRoot $ArchName
    $TolkSupport = Join-Path $TolkSupportRoot $ArchName
    Require-File $ComponentDll
    Require-File (Join-Path $TolkBuild "Tolk.dll")
    Require-Directory $TolkSupport

    $TolkTarget = Join-Path $TargetDir "tolk"
    New-Item -ItemType Directory -Force -Path $TargetDir,$TolkTarget | Out-Null
    Copy-Item $ComponentDll $TargetDir -Force
    $TolkTargetPath = Join-Path $TolkTarget $TolkFileName
    Copy-Item (Join-Path $TolkBuild "Tolk.dll") $TolkTargetPath -Force
    Assert-PeArchitecture $ComponentDll $ArchName
    Assert-PeArchitecture $TolkTargetPath $ArchName
    Assert-TolkDriverNames $TolkTargetPath $ArchName

    $DriverMap = Get-DriverMap $ArchName
    foreach ($SourceName in $DriverMap.Keys) {
        $SourcePath = Join-Path $TolkSupport $SourceName
        $TargetName = $DriverMap[$SourceName]
        $TargetPath = Join-Path $TolkTarget $TargetName
        Require-File $SourcePath
        Copy-Item $SourcePath $TargetPath -Force
        Assert-PeArchitecture $TargetPath $ArchName
    }

    Get-ChildItem -LiteralPath $TolkSupport -File | ForEach-Object {
        if ($_.Name -eq "Tolk.dll" -or $DriverMap.ContainsKey($_.Name)) {
            return
        }
        if ($_.Extension -in ".ini", ".conf") {
            Copy-Item $_.FullName (Join-Path $TolkTarget $_.Name) -Force
        }
    }

    $OriginalNames = @("Tolk.dll") + @($DriverMap.Keys)
    foreach ($OriginalName in $OriginalNames) {
        if (Test-Path (Join-Path $TolkTarget $OriginalName)) {
            throw "Unnamespaced runtime file was packaged: $OriginalName"
        }
    }

    Require-File (Join-Path $TolkTarget $TolkFileName)
}

New-Item -ItemType Directory -Force -Path $DistRoot | Out-Null
Remove-Item -Recurse -Force `
    (Join-Path $DistRoot "foo_output_device_switcher"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x64"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x86"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k1.5-1.6-x86"), `
    (Join-Path $DistRoot "foo_output_device_switcher-package") `
    -ErrorAction SilentlyContinue
Remove-Item -Force `
    (Join-Path $DistRoot "foo_output_device_switcher.fb2k-component"), `
    (Join-Path $DistRoot "foo_output_device_switcher.zip"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x64.fb2k-component"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x86.fb2k-component"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86.fb2k-component"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k1.5-1.6-x86.fb2k-component"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x64.zip"), `
    (Join-Path $DistRoot "foo_output_device_switcher-x86.zip"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86.zip"), `
    (Join-Path $DistRoot "foo_output_device_switcher-fb2k1.5-1.6-x86.zip") `
    -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $DistRoot -Filter "foo_output_device_switcher*.fb2k-component" -File | Remove-Item -Force

$ManualX64 = Join-Path $DistRoot "foo_output_device_switcher-x64"
Copy-Payload $ManualX64 "x64" "x64"
$X64Zip = Join-Path $DistRoot "foo_output_device_switcher-$Version-x64.zip"
$X64Package = Join-Path $DistRoot "foo_output_device_switcher-$Version-x64.fb2k-component"
Compress-Archive -Path (Join-Path $ManualX64 "*") -DestinationPath $X64Zip
Move-Item $X64Zip $X64Package -Force

$ManualX86 = Join-Path $DistRoot "foo_output_device_switcher-x86"
Copy-Payload $ManualX86 "x86" "Win32"
$X86Zip = Join-Path $DistRoot "foo_output_device_switcher-$Version-x86.zip"
$X86Package = Join-Path $DistRoot "foo_output_device_switcher-$Version-x86.fb2k-component"
Compress-Archive -Path (Join-Path $ManualX86 "*") -DestinationPath $X86Zip
Move-Item $X86Zip $X86Package -Force

Remove-Item -LiteralPath $ManualX64,$ManualX86 -Recurse -Force

Write-Host "x64 package: $X64Package"
Write-Host "x86 package: $X86Package"
