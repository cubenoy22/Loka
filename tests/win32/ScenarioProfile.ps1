function Read-ScenarioProfile([string]$Path) {
    $values = @{}
    foreach ($line in [System.IO.File]::ReadAllLines($Path)) {
        if ($line -notmatch '^([a-z_]+)=(.*)$') {
            throw "profile stage failed: invalid profile line '$line' in $Path"
        }
        if ($values.ContainsKey($Matches[1])) {
            throw "profile stage failed: duplicate profile field '$($Matches[1])' in $Path"
        }
        $values[$Matches[1]] = $Matches[2]
    }
    return $values
}

function Get-CaptureProfileMismatch($Expected, $Actual) {
    $fields = @($Expected.Keys) + @($Actual.Keys)
    foreach ($field in ($fields | Sort-Object -Unique)) {
        $expectedHasField = $Expected.ContainsKey($field)
        $actualHasField = $Actual.ContainsKey($field)
        if ($expectedHasField -ne $actualHasField `
            -or ($expectedHasField -and $Expected[$field] -cne $Actual[$field])) {
            return $field
        }
    }
    return $null
}
