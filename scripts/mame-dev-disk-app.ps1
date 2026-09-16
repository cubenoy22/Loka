param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Key,
    [switch]$Target,
    [switch]$Build,
    [switch]$BuildAndPrepare
)

$ErrorActionPreference = "Stop"
$workspace = Split-Path -Parent $PSScriptRoot
$preset = "retro68-68k-release"
$data = $null

switch ($Key) {
    "HelloWorld" { $cmakeTarget = "LokaHello68K_APPL"; $bin = "build/retro68/68k/Release/example/HelloWorld/LokaHello68K.bin" }
    "SmirkyCard" { $cmakeTarget = "LokaSmirkyCard68K_APPL"; $bin = "build/retro68/68k/Release/example/SmirkyCard/LokaSmirkyCard68K.bin"; $data = "example/SmirkyCard/MAIN.JS" }
    "MineSweeper" { $cmakeTarget = "LokaMine68K_APPL"; $bin = "build/retro68/68k/Release/example/MineSweeper/LokaMine68K.bin" }
    "SimpleViewer" { $cmakeTarget = "LokaSimpleViewer68K_APPL"; $bin = "build/retro68/68k/Release/example/SimpleViewer/LokaSimpleViewer68K.bin" }
    "FloppyBird" { $cmakeTarget = "LokaFloppyBird68K_APPL"; $bin = "build/retro68/68k/Release/example/FloppyBird/LokaFloppyBird68K.bin" }
    "SmirkBench" { $cmakeTarget = "LokaSmirkBench68K_APPL"; $bin = "build/retro68/68k/Release/example/SmirkBench/LokaSmirkBench68K.bin" }
    "LazyList" { $cmakeTarget = "LokaLazyList68K_APPL"; $bin = "build/retro68/68k/Release/example/LazyList/LokaLazyList68K.bin" }
    "Tutorial" { $cmakeTarget = "LokaTutorial68K_APPL"; $bin = "build/retro68/68k/Release/example/Tutorial/LokaTutorial68K.bin" }
    "ScrapbookUI" { $cmakeTarget = "ScrapbookUI68K_APPL"; $bin = "build/retro68/68k/Release/example/ScrapbookUI/ScrapbookUI68K.bin"; $data = "build/retro68/68k/Release/example/ScrapbookUI/ASSETS.LRP" }
    "ScrapbookStandaloneFlow" { $cmakeTarget = "LokaScrapbookStandaloneFlow68K_APPL"; $bin = "build/presentation/toolbox-68k-release/LokaScrapbookStandaloneFlow68K.bin"; $data = "build/presentation/toolbox-68k-release/ASSETS.LRP" }
    "HelloWorldStandaloneFlow" { $cmakeTarget = "LokaHelloStandaloneFlow68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaHelloStandaloneFlow68K.bin" }
    "TutorialStandaloneFlow" { $cmakeTarget = "LokaTutorialStandaloneFlow68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaTutorialStandaloneFlow68K.bin" }
    "MineSweeperStandaloneFlow" { $cmakeTarget = "LokaMineStandaloneFlow68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaMineStandaloneFlow68K.bin" }
    "FloppyBirdStandaloneFlow" { $cmakeTarget = "LokaFloppyStandaloneFlow68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaFloppyStandaloneFlow68K.bin" }
    "HelloWorldScenarioLoop" { $cmakeTarget = "LokaHelloScenarioLoop68K_APPL"; $bin = "build/retro68/68k/Release/tests/toolbox/LokaHelloScenarioLoop68K.bin" }
    "MineSweeperScenarioLoop" { $cmakeTarget = "LokaMineScenarioLoop68K_APPL"; $bin = "build/retro68/68k/Release/tests/toolbox/LokaMineScenarioLoop68K.bin" }
    default { throw "unknown SCSI app key: $Key" }
}

function Build-App {
    if ($Key -eq "ScrapbookStandaloneFlow") {
        & bash "$PSScriptRoot/toolbox-standalone-flow.sh" Stage
    } else {
        if ($Key -eq "SmirkyCard") {
            & bash "$PSScriptRoot/retro68-cmake.sh" --preset $preset -DLOKA_BUILD_SMIRKYCARD=ON
        } else {
            & bash "$PSScriptRoot/retro68-cmake.sh" --preset $preset
        }
        if ($LASTEXITCODE -eq 0) {
            & bash "$PSScriptRoot/retro68-cmake.sh" --build --preset $preset --target $cmakeTarget
        }
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Target) {
    Write-Output $cmakeTarget
} elseif ($Build) {
    Build-App
} else {
    if ($BuildAndPrepare) { Build-App }
    $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$PSScriptRoot/mame-dev-disk.ps1", (Join-Path $workspace $bin))
    if ($data) { $arguments += (Join-Path $workspace $data) }
    & powershell.exe @arguments
    exit $LASTEXITCODE
}
