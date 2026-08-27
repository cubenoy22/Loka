$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$RigDescriptorSupport = Join-Path $ProjectDirectory "tests/win32/RigDescriptor.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-rig-descriptor-" + [Guid]::NewGuid())

. $RigDescriptorSupport

function Expect-Refusal([string]$Directory, [string]$Name, [string]$Fragment, [string]$Case) {
    try {
        $resolved = Resolve-RigDescriptorPath $Directory $Name
    } catch {
        if ($_.Exception.Message -notlike "profile stage failed:*") {
            throw "$Case refused outside the profile stage: $($_.Exception.Message)"
        }
        if ($_.Exception.Message -notlike "*$Fragment*") {
            throw "$Case refused without '$Fragment': $($_.Exception.Message)"
        }
        return
    }
    throw "$Case resolved to '$resolved' instead of refusing."
}

try {
    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    $declared = Join-Path $TestRoot "declared-rig.ini"
    Set-Content -LiteralPath $declared -Value "[rig]" -Encoding Ascii

    # A declared machine resolves to its own descriptor and nothing else.
    $resolved = Resolve-RigDescriptorPath $TestRoot "declared-rig"
    if ($resolved -cne $declared) {
        throw "A declared rig resolved to '$resolved' instead of '$declared'."
    }

    # Unset is the case that used to be impossible: the rail always had an
    # answer because it read one off the architecture. Silence must refuse.
    Expect-Refusal $TestRoot "" "LOKA_WIN32_RIG is unset" "An unset rig name"
    Expect-Refusal $TestRoot $null "LOKA_WIN32_RIG is unset" "A null rig name"

    # The name becomes a path, so it may not be able to leave the directory or
    # name a file the rail does not track.
    Expect-Refusal $TestRoot "../evil" "invalid LOKA_WIN32_RIG name" "A traversing rig name"
    Expect-Refusal $TestRoot "a..b" "invalid LOKA_WIN32_RIG name" "A rig name containing '..'"
    Expect-Refusal $TestRoot "sub/rig" "invalid LOKA_WIN32_RIG name" "A rig name with a separator"
    Expect-Refusal $TestRoot "sub\rig" "invalid LOKA_WIN32_RIG name" "A rig name with a backslash"
    Expect-Refusal $TestRoot "Declared-Rig" "invalid LOKA_WIN32_RIG name" "An upper-case rig name"
    Expect-Refusal $TestRoot "-leading" "invalid LOKA_WIN32_RIG name" "A rig name starting with '-'"

    # A well-formed name for a machine nobody declared says which file to write,
    # because that is the whole remedy.
    Expect-Refusal $TestRoot "undeclared-rig" `
        (Join-Path $TestRoot "undeclared-rig.ini") "An undeclared rig name"
    Expect-Refusal $TestRoot "undeclared-rig" "local.example.ini" `
        "An undeclared rig name"

    # Where the runner looks. An override is honoured verbatim, and without one
    # the directory is the operator's, not the repository's -- the whole point of
    # the change. Without this the resolver could be correct about names and
    # still be pointed at a directory inside the tree.
    $savedHome = $env:LOKA_WIN32_RIG_HOME
    try {
        $env:LOKA_WIN32_RIG_HOME = $TestRoot
        $overridden = Get-RigDescriptorDirectory
        if ($overridden -cne $TestRoot) {
            throw "LOKA_WIN32_RIG_HOME=$TestRoot resolved to '$overridden'."
        }
        $env:LOKA_WIN32_RIG_HOME = ""
        $fallback = Get-RigDescriptorDirectory
        if ($fallback.StartsWith($ProjectDirectory, [StringComparison]::OrdinalIgnoreCase)) {
            throw "The default rig directory '$fallback' is inside the repository."
        }
        if ($fallback -notlike "*.config*loka*rigs*win32") {
            throw "The default rig directory '$fallback' is not the local rig directory."
        }
    } finally {
        $env:LOKA_WIN32_RIG_HOME = $savedHome
    }

    # The example is tracked so an operator has a shape to copy, and the refusal
    # for a missing descriptor points at it. It must therefore exist.
    $example = Join-Path $ProjectDirectory "scripts/rig/win32/rigs/local.example.ini"
    if (-not (Test-Path -LiteralPath $example -PathType Leaf)) {
        throw "The refusal names $example, which does not exist."
    }

    Write-Output "Win32 rig descriptors are named, never derived."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
