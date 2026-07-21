$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$DistRoot = Join-Path $Root "dist"
$TolkRoot = Join-Path $Root "third_party\tolk-with-zdsr"
$MainSource = Get-Content (Join-Path $Root "third_party\foobar2000-sdk\foobar2000\foo_output_device_switcher\main.cpp") -Raw -Encoding UTF8
$VersionMatch = [regex]::Match($MainSource, 'DECLARE_COMPONENT_VERSION\([^,]+,\s*"([^"]+)"')
if (-not $VersionMatch.Success) { throw "Unable to read component version from main.cpp." }
$Version = $VersionMatch.Groups[1].Value

function Require-File($Path) {
    if (-not (Test-Path $Path)) { throw "Missing file: $Path" }
}

function Require-Directory($Path) {
    if (-not (Test-Path $Path)) { throw "Missing directory: $Path" }
}

function Replace-FixedByteSequence(
    [byte[]]$Data,
    [byte[]]$OldBytes,
    [byte[]]$NewBytes,
    [string]$Description
) {
    if ($OldBytes.Length -ne $NewBytes.Length) {
        throw "Replacement length mismatch for ${Description}: $($OldBytes.Length) != $($NewBytes.Length)"
    }

    $Matches = 0
    for ($Index = 0; $Index -le $Data.Length - $OldBytes.Length; $Index++) {
        $Equal = $true
        for ($Offset = 0; $Offset -lt $OldBytes.Length; $Offset++) {
            if ($Data[$Index + $Offset] -ne $OldBytes[$Offset]) {
                $Equal = $false
                break
            }
        }
        if (-not $Equal) { continue }

        [Array]::Copy($NewBytes, 0, $Data, $Index, $NewBytes.Length)
        $Matches++
        $Index += $OldBytes.Length - 1
    }

    if ($Matches -ne 1) {
        throw "Expected exactly one ${Description} occurrence in Tolk.dll, found $Matches."
    }
}

function Write-Namespaced-Tolk($Source, $Destination, $ArchName) {
    Require-File $Source
    $Data = [IO.File]::ReadAllBytes($Source)

    if ($ArchName -eq "x64") {
        $Replacements = @(
            @("nvdaControllerClient64.dll", "ods-nvda-client-64bits.dll"),
            @("byctrl-x64.dll", "ods-br-x64.dll"),
            @("ZDSRAPI_x64.dll", "ods-zsr_x64.dll")
        )
    } elseif ($ArchName -eq "x86") {
        $Replacements = @(
            @("nvdaControllerClient32.dll", "ods-nvda-client-32bits.dll"),
            @("byctrl.dll", "ods-br.dll"),
            @("ZDSRAPI.dll", "ods-zsr.dll")
        )
    } else {
        throw "Unsupported Tolk architecture: $ArchName"
    }

    foreach ($Replacement in $Replacements) {
        $OldName = $Replacement[0]
        $NewName = $Replacement[1]
        Replace-FixedByteSequence $Data `
            ([Text.Encoding]::ASCII.GetBytes($OldName)) `
            ([Text.Encoding]::ASCII.GetBytes($NewName)) `
            "ASCII '$OldName'"
        Replace-FixedByteSequence $Data `
            ([Text.Encoding]::Unicode.GetBytes($OldName)) `
            ([Text.Encoding]::Unicode.GetBytes($NewName)) `
            "UTF-16 '$OldName'"
    }

    [IO.File]::WriteAllBytes($Destination, $Data)
}

function Copy-Payload($TargetDir, $ArchName, $ComponentPlatform) {
    $ComponentDll = Join-Path $Root "build\foo_output_device_switcher\Release\$ComponentPlatform\foo_output_device_switcher.dll"
    $TolkSource = Join-Path $TolkRoot $ArchName
    Require-File $ComponentDll
    Require-Directory $TolkSource

    $TolkTarget = Join-Path $TargetDir "tolk"
    New-Item -ItemType Directory -Force -Path $TargetDir,$TolkTarget | Out-Null
    Copy-Item $ComponentDll $TargetDir -Force
    Write-Namespaced-Tolk (Join-Path $TolkSource "Tolk.dll") (Join-Path $TolkTarget "TolkODS.dll") $ArchName

    Get-ChildItem $TolkSource -File | ForEach-Object {
        $TargetName = $_.Name
        switch ($_.Name) {
            "Tolk.dll" { return }
            "nvdaControllerClient64.dll" { $TargetName = "ods-nvda-client-64bits.dll" }
            "nvdaControllerClient32.dll" { $TargetName = "ods-nvda-client-32bits.dll" }
            "byctrl-x64.dll" { $TargetName = "ods-br-x64.dll" }
            "byctrl.dll" { $TargetName = "ods-br.dll" }
            "ZDSRAPI_x64.dll" { $TargetName = "ods-zsr_x64.dll" }
            "ZDSRAPI.dll" { $TargetName = "ods-zsr.dll" }
        }
        Copy-Item $_.FullName (Join-Path $TolkTarget $TargetName) -Force
    }

    foreach ($OriginalName in "Tolk.dll", "nvdaControllerClient64.dll", "nvdaControllerClient32.dll", "byctrl-x64.dll", "byctrl.dll", "ZDSRAPI_x64.dll", "ZDSRAPI.dll") {
        if (Test-Path (Join-Path $TolkTarget $OriginalName)) {
            throw "Unnamespaced runtime file was packaged: $OriginalName"
        }
    }
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
