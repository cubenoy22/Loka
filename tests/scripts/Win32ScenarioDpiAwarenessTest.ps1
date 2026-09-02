param(
    [Parameter(Mandatory = $true)]
    [string]$Source,
    [Parameter(Mandatory = $true)]
    [string]$TestSource
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

Add-Type -AssemblyName System.Drawing
Add-Type -Path $Source, $TestSource -ReferencedAssemblies System.Drawing
[LokaScenarioCaptureNativeTests]::Run()
Write-Output "Win32 scenario DPI awareness tests passed"
