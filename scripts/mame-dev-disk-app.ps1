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
    { $_ -in @("AllStandaloneLoops", "AllStandaloneFlows") } {
        if ($Target) { throw "$Key has no single application target" }
        $preset = "retro68-68k-standalone-release"
        if ($Key -eq "AllStandaloneLoops") {
            $batchTarget = "LokaStandaloneLoop68KAll"
            $diskOption = "--all-loops"
        } else {
            $batchTarget = "LokaStandaloneFlow68KAll"
            $diskOption = "--all-flows"
        }
        if ($Build -or $BuildAndPrepare) {
            & bash "$PSScriptRoot/retro68-cmake.sh" --preset $preset
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            & bash "$PSScriptRoot/retro68-cmake.sh" --build --preset $preset --target $batchTarget
            if ($LASTEXITCODE -ne 0 -or $Build) { exit $LASTEXITCODE }
        }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/mame-dev-disk.ps1" -MacBinaryPath $diskOption
        exit $LASTEXITCODE
    }
    "AllLoops" {
        if (-not $Build -or $Target -or $BuildAndPrepare) {
            throw "AllLoops is available only with -Build"
        }
        foreach ($loop in @("ScrapbookStandaloneLoop", "HelloWorldStandaloneLoop", "TutorialStandaloneLoop", "MineSweeperStandaloneLoop", "FloppyBirdStandaloneLoop", "ScrapbookStandaloneFlow", "HelloWorldStandaloneFlow", "TutorialStandaloneFlow", "MineSweeperStandaloneFlow", "FloppyBirdStandaloneFlow", "HelloWorldScenarioLoop", "MineSweeperScenarioLoop")) {
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath $loop -Build
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        exit 0
    }
    "ScrapbookStandaloneLoop" { $cmakeTarget = "LokaScrapbookStandaloneLoop68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaScrapbookStandaloneLoop68K.bin"; $data = "build/retro68/68k/Standalone/Release/tests/toolbox/ASSETS.LRP" }
    "HelloWorldStandaloneLoop" { $cmakeTarget = "LokaHelloStandaloneLoop68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaHelloStandaloneLoop68K.bin" }
    "TutorialStandaloneLoop" { $cmakeTarget = "LokaTutorialStandaloneLoop68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaTutorialStandaloneLoop68K.bin" }
    "MineSweeperStandaloneLoop" { $cmakeTarget = "LokaMineStandaloneLoop68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaMineStandaloneLoop68K.bin" }
    "FloppyBirdStandaloneLoop" { $cmakeTarget = "LokaFloppyStandaloneLoop68K_APPL"; $preset = "retro68-68k-standalone-release"; $bin = "build/retro68/68k/Standalone/Release/tests/toolbox/LokaFloppyStandaloneLoop68K.bin" }
    "All" {
        if ($Target) { throw "All has no single target" }
        if ($Build -or $BuildAndPrepare) {
            $configureArguments = @("--preset", $preset)
            if ($BuildAndPrepare) { $configureArguments += "-DLOKA_BUILD_SMIRKYCARD=ON" }
            & bash "$PSScriptRoot/retro68-cmake.sh" @configureArguments
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            & bash "$PSScriptRoot/retro68-cmake.sh" --build --preset $preset
            if ($LASTEXITCODE -ne 0 -or $Build) { exit $LASTEXITCODE }
        }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/mame-dev-disk.ps1" -MacBinaryPath "--all"
        exit $LASTEXITCODE
    }
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
