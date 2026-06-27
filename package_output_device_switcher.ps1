$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$DistRoot = Join-Path $Root "dist"
$TolkRoot = Join-Path $Root "third_party\tolk-with-zdsr"

function Require-File($Path) {
    if (-not (Test-Path $Path)) { throw "Missing file: $Path" }
}

function Require-Directory($Path) {
    if (-not (Test-Path $Path)) { throw "Missing directory: $Path" }
}

function Copy-Payload($TargetDir, $ArchName, $ComponentPlatform) {
    $ComponentDll = Join-Path $Root "build\foo_output_device_switcher\Release\$ComponentPlatform\foo_output_device_switcher.dll"
    $TolkSource = Join-Path $TolkRoot $ArchName
    Require-File $ComponentDll
    Require-Directory $TolkSource

    $TolkTarget = Join-Path $TargetDir "tolk"
    New-Item -ItemType Directory -Force -Path $TargetDir,$TolkTarget | Out-Null
    Copy-Item $ComponentDll $TargetDir -Force
    Get-ChildItem $TolkSource -File | ForEach-Object {
        Copy-Item $_.FullName $TolkTarget -Force
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

$ManualX64 = Join-Path $DistRoot "foo_output_device_switcher-x64"
Copy-Payload $ManualX64 "x64" "x64"
Compress-Archive -Path (Join-Path $ManualX64 "*") -DestinationPath (Join-Path $DistRoot "foo_output_device_switcher-x64.zip")
Move-Item (Join-Path $DistRoot "foo_output_device_switcher-x64.zip") (Join-Path $DistRoot "foo_output_device_switcher-x64.fb2k-component") -Force

$LegacyX86 = Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86"
Copy-Payload $LegacyX86 "x86" "Win32"
Compress-Archive -Path (Join-Path $LegacyX86 "*") -DestinationPath (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86.zip")
Move-Item (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86.zip") (Join-Path $DistRoot "foo_output_device_switcher-fb2k-x86.fb2k-component") -Force

$PackageDir = Join-Path $DistRoot "foo_output_device_switcher-package"
Copy-Payload $PackageDir "x86" "Win32"
Copy-Payload (Join-Path $PackageDir "x64") "x64" "x64"
Compress-Archive -Path (Join-Path $PackageDir "*") -DestinationPath (Join-Path $DistRoot "foo_output_device_switcher.zip")
Move-Item (Join-Path $DistRoot "foo_output_device_switcher.zip") (Join-Path $DistRoot "foo_output_device_switcher.fb2k-component") -Force

Write-Host "Manual x64 folder: $ManualX64"
Write-Host "foobar2000 1.5/1.6 x86 folder: $LegacyX86"
Write-Host "x64 package: $(Join-Path $DistRoot 'foo_output_device_switcher-x64.fb2k-component')"
Write-Host "foobar2000 1.5/1.6 x86 package: $(Join-Path $DistRoot 'foo_output_device_switcher-fb2k-x86.fb2k-component')"
Write-Host "combined package: $(Join-Path $DistRoot 'foo_output_device_switcher.fb2k-component')"
