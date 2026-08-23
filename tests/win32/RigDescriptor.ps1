# Which rig is this? The rail cannot work it out on its own, and the answer
# does not belong to the repository.
#
# It used to derive the answer from the architecture the vehicle reported --
# arch=x64 chose win32-x64.ini, a tracked file -- on the reasoning that one
# architecture meant one machine. That is false, and it was measured to be
# false: an isolated Windows 11 guest reports a capture profile byte-identical
# to the reference machine's, down to the window's pixel dimensions, and settles
# on a different picture (#459). Under derivation the guest named itself the
# reference machine and the capture guard passed it.
#
# So the rig is named, never inferred, and its descriptor lives beside the
# operator rather than in the tree. That is the same rule the goldens already
# follow: build/win32-scenario/golden is ignored, because a golden is a fact
# about one machine. A descriptor is the same kind of fact. Tracking it also
# made everyone who cloned this repository carry someone else's rig.
function Get-RigDescriptorDirectory() {
    if (-not [string]::IsNullOrEmpty($env:LOKA_WIN32_RIG_HOME)) {
        return $env:LOKA_WIN32_RIG_HOME
    }
    $home_ = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
    if ([string]::IsNullOrEmpty($home_)) {
        throw ("profile stage failed: cannot locate the local rig directory; this machine " +
            "reports no user profile. Set LOKA_WIN32_RIG_HOME to the directory holding " +
            "your rig descriptors.")
    }
    return (Join-Path (Join-Path (Join-Path $home_ ".config") "loka") "rigs\win32")
}

function Resolve-RigDescriptorPath([string]$Directory, [string]$Name) {
    if ([string]::IsNullOrEmpty($Name)) {
        throw ("profile stage failed: LOKA_WIN32_RIG is unset; set LOKA_WIN32_RIG=<name> " +
            "for a descriptor in $Directory")
    }
    # -cnotmatch, not -notmatch: PowerShell's default match is case-insensitive,
    # so the lower-case rule would pass 'Omen' and Test-Path would then resolve it
    # on a case-insensitive filesystem. The macOS rail's =~ is case-sensitive; this
    # keeps the two rails agreeing on what a rig name is.
    if ($Name -cnotmatch '^[a-z0-9][a-z0-9.-]*$' -or $Name.Contains("..")) {
        throw ("profile stage failed: invalid LOKA_WIN32_RIG name '$Name'; expected a name " +
            "matching ^[a-z0-9][a-z0-9.-]*$ without '..'")
    }
    $descriptor = Join-Path $Directory "$Name.ini"
    if (-not (Test-Path -LiteralPath $descriptor -PathType Leaf)) {
        throw ("profile stage failed: no rig descriptor at $descriptor; this rail's goldens " +
            "would be pinned to an undeclared environment. Copy " +
            "scripts/rig/win32/rigs/local.example.ini there and declare the [capture] fields " +
            "this machine reports. A run only reports them once the descriptor exists, so " +
            "declare the machine first and read them back from the actual.profile the " +
            "refused run leaves beside its capture.")
    }
    return $descriptor
}
