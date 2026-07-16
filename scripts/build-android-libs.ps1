# Build Android prebuilt libs via ya (after_build hook copies .a automatically)

param(
    [string]$NdkHome = $env:ANDROID_NDK_HOME
)

$ErrorActionPreference = "Stop"

if (-not $NdkHome) {
    $NdkHome = $env:ANDROID_NDK_ROOT
}
if (-not $NdkHome) {
    Write-Error "Set ANDROID_NDK_HOME to your Android NDK path."
}

$env:ANDROID_NDK_HOME = $NdkHome

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$abis = @("arm64-v8a", "armeabi-v7a")

foreach ($abi in $abis) {
    Write-Host "=== ya -p android -a $abi -b yui-android-prebuilt ===" -ForegroundColor Cyan
    ya -p android -a $abi -m release -b yui-android-prebuilt
}

Write-Host "Done -> third_party/yui-prebuilt/android/" -ForegroundColor Green
