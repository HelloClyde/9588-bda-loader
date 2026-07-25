# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
    [string]$Output,
    [switch]$Diagnostic,
    [string]$Prefix
)

$ErrorActionPreference = "Stop"
$BuildArgs = @()
if ($Output) {
    $BuildArgs += @("--output", $Output)
}
if ($Diagnostic) {
    $BuildArgs += "--diagnostic"
}
if ($Prefix) {
    $BuildArgs += @("--prefix", $Prefix)
}

python (Join-Path $PSScriptRoot "build.py") @BuildArgs
if ($LASTEXITCODE -ne 0) {
    throw "BDA Loader build failed with exit code $LASTEXITCODE"
}
