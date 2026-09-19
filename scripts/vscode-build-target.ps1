param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("win32")]
    [string]$Platform,
    [Parameter(Mandatory = $true, Position = 1)]
    [string]$Key
)

$ErrorActionPreference = "Stop"
$preset = "win32-debug"
$target = $null
$configureArguments = @()

switch ($Key) {
    "AllLoops" {
        foreach ($loop in @("ScrapbookStandaloneLoop", "HelloWorldStandaloneLoop", "TutorialStandaloneLoop", "MineSweeperStandaloneLoop", "FloppyBirdStandaloneLoop", "HelloWorldScenarioLoop", "MineSweeperScenarioLoop")) {
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath win32 $loop
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        exit 0
    }
    "All" {}
    "HelloWorld" { $target = "LokaHelloWin32" }
    "MineSweeper" { $target = "LokaMineWin32" }
    "SimpleViewer" { $target = "LokaSimpleViewerWin32" }
    "ScrapbookUI" { $target = "ScrapbookUIWin32" }
    "FloppyBird" { $target = "LokaFloppyBirdWin32" }
    "SmirkBench" { $target = "LokaSmirkBenchWin32" }
    "LazyList" { $target = "LokaLazyListWin32" }
    "Tutorial" { $target = "LokaTutorialWin32" }
    "SmirkyCard" { $target = "LokaSmirkyCardWin32"; $configureArguments += "-DLOKA_BUILD_SMIRKYCARD=ON" }
    "Tests" { $preset = "win32-tests"; $target = "LokaTestsWin32" }
    "HelloWorldScenarioLoop" { $target = "LokaHelloWorldScenarioLoopWin32" }
    "ScrapbookStandaloneLoop" { $preset = "win32-standalone-debug"; $target = "LokaScrapbookStandaloneLoopWin32" }
    "HelloWorldStandaloneLoop" { $preset = "win32-standalone-debug"; $target = "LokaHelloWorldStandaloneLoopWin32" }
    "TutorialStandaloneLoop" { $preset = "win32-standalone-debug"; $target = "LokaTutorialStandaloneLoopWin32" }
    "MineSweeperStandaloneLoop" { $preset = "win32-standalone-debug"; $target = "LokaMineSweeperStandaloneLoopWin32" }
    "FloppyBirdStandaloneLoop" { $preset = "win32-standalone-debug"; $target = "LokaFloppyBirdStandaloneLoopWin32" }
    "MineSweeperScenarioLoop" { $target = "LokaMineSweeperScenarioLoopWin32" }
    default { throw "unknown Win32 build key: $Key" }
}

& cmake --preset $preset @configureArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildArguments = @("--build", "--preset", $preset)
if ($preset -eq "win32-standalone-debug") {
    $buildArguments = @("--build", "--preset", "win32-standalone-loop")
}
if ($target) { $buildArguments += @("--target", $target) }
& cmake @buildArguments
exit $LASTEXITCODE
