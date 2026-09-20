param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("win32")]
    [string]$Platform,
    [Parameter(Mandatory = $true, Position = 1)]
    [string]$Key
)

$ErrorActionPreference = "Stop"
$configurePreset = "win32-debug"
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
    "Tests" { $target = "LokaTestsWin32" }
    "HelloWorldScenarioLoop" { $target = "LokaHelloWorldScenarioLoopWin32" }
    "ScrapbookStandaloneLoop" { $configurePreset = "win32-standalone-debug"; $target = "LokaScrapbookStandaloneLoopWin32" }
    "HelloWorldStandaloneLoop" { $configurePreset = "win32-standalone-debug"; $target = "LokaHelloWorldStandaloneLoopWin32" }
    "TutorialStandaloneLoop" { $configurePreset = "win32-standalone-debug"; $target = "LokaTutorialStandaloneLoopWin32" }
    "MineSweeperStandaloneLoop" { $configurePreset = "win32-standalone-debug"; $target = "LokaMineSweeperStandaloneLoopWin32" }
    "FloppyBirdStandaloneLoop" { $configurePreset = "win32-standalone-debug"; $target = "LokaFloppyBirdStandaloneLoopWin32" }
    "MineSweeperScenarioLoop" { $target = "LokaMineSweeperScenarioLoopWin32" }
    default { throw "unknown Win32 build key: $Key" }
}

$buildPreset = $configurePreset
if ($Key -eq "Tests") { $buildPreset = "win32-tests" }
if ($configurePreset -eq "win32-standalone-debug") {
    $buildPreset = "win32-standalone-loop"
}
& cmake --preset $configurePreset @configureArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildArguments = @("--build", "--preset", $buildPreset)
if ($target) { $buildArguments += @("--target", $target) }
& cmake @buildArguments
exit $LASTEXITCODE
