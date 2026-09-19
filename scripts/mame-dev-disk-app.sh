#!/usr/bin/env bash

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
workspace_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

usage() {
    echo "usage: $0 [--target|--build|--build-and-prepare] <key>" >&2
    exit 2
}

mode=prepare
if [ "${1-}" = "--target" ] || [ "${1-}" = "--build" ] || [ "${1-}" = "--build-and-prepare" ]; then
    mode=$1
    shift
fi
[ "$#" -eq 1 ] || usage
key=$1

target=
preset=retro68-68k-release
bin=
data=

case "$key" in
    AllStandaloneLoops|AllStandaloneFlows)
        [ "$mode" != "--target" ] || usage
        preset=retro68-68k-standalone-release
        if [ "$key" = "AllStandaloneLoops" ]; then
            target=LokaStandaloneLoop68KAll
            disk_option=--all-loops
        else
            target=LokaStandaloneFlow68KAll
            disk_option=--all-flows
        fi
        if [ "$mode" = "--build" ] || [ "$mode" = "--build-and-prepare" ]; then
            "$script_dir/retro68-cmake.sh" --preset "$preset"
            "$script_dir/retro68-cmake.sh" --build --preset "$preset" --target "$target"
        fi
        [ "$mode" != "--build" ] || exit 0
        exec "$script_dir/mame-dev-disk.sh" "$disk_option"
        ;;
    AllLoops)
        [ "$mode" = "--build" ] || usage
        for loop in ScrapbookStandaloneLoop HelloWorldStandaloneLoop TutorialStandaloneLoop MineSweeperStandaloneLoop FloppyBirdStandaloneLoop ScrapbookStandaloneFlow HelloWorldStandaloneFlow TutorialStandaloneFlow MineSweeperStandaloneFlow FloppyBirdStandaloneFlow HelloWorldScenarioLoop MineSweeperScenarioLoop; do
            bash "${BASH_SOURCE[0]}" --build "$loop"
        done
        exit 0
        ;;
    ScrapbookStandaloneLoop)
        target=LokaScrapbookStandaloneLoop68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaScrapbookStandaloneLoop68K.bin
        data=build/retro68/68k/Standalone/Release/tests/toolbox/ASSETS.LRP
        ;;
    HelloWorldStandaloneLoop)
        target=LokaHelloStandaloneLoop68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaHelloStandaloneLoop68K.bin
        ;;
    TutorialStandaloneLoop)
        target=LokaTutorialStandaloneLoop68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaTutorialStandaloneLoop68K.bin
        ;;
    MineSweeperStandaloneLoop)
        target=LokaMineStandaloneLoop68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaMineStandaloneLoop68K.bin
        ;;
    FloppyBirdStandaloneLoop)
        target=LokaFloppyStandaloneLoop68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaFloppyStandaloneLoop68K.bin
        ;;
    All)
        [ "$mode" != "--target" ] || usage
        if [ "$mode" = "--build" ]; then
            "$script_dir/retro68-cmake.sh" --preset "$preset"
            exec "$script_dir/retro68-cmake.sh" --build --preset "$preset"
        fi
        if [ "$mode" = "--build-and-prepare" ]; then
            "$script_dir/retro68-cmake.sh" --preset "$preset" -DLOKA_BUILD_SMIRKYCARD=ON
            "$script_dir/retro68-cmake.sh" --build --preset "$preset"
        fi
        exec "$script_dir/mame-dev-disk.sh" --all
        ;;
    HelloWorld)
        target=LokaHello68K_APPL
        bin=build/retro68/68k/Release/example/HelloWorld/LokaHello68K.bin
        ;;
    SmirkyCard)
        target=LokaSmirkyCard68K_APPL
        bin=build/retro68/68k/Release/example/SmirkyCard/LokaSmirkyCard68K.bin
        data=example/SmirkyCard/MAIN.JS
        ;;
    MineSweeper)
        target=LokaMine68K_APPL
        bin=build/retro68/68k/Release/example/MineSweeper/LokaMine68K.bin
        ;;
    SimpleViewer)
        target=LokaSimpleViewer68K_APPL
        bin=build/retro68/68k/Release/example/SimpleViewer/LokaSimpleViewer68K.bin
        ;;
    FloppyBird)
        target=LokaFloppyBird68K_APPL
        bin=build/retro68/68k/Release/example/FloppyBird/LokaFloppyBird68K.bin
        ;;
    SmirkBench)
        target=LokaSmirkBench68K_APPL
        bin=build/retro68/68k/Release/example/SmirkBench/LokaSmirkBench68K.bin
        ;;
    LazyList)
        target=LokaLazyList68K_APPL
        bin=build/retro68/68k/Release/example/LazyList/LokaLazyList68K.bin
        ;;
    Tutorial)
        target=LokaTutorial68K_APPL
        bin=build/retro68/68k/Release/example/Tutorial/LokaTutorial68K.bin
        ;;
    ScrapbookUI)
        target=ScrapbookUI68K_APPL
        bin=build/retro68/68k/Release/example/ScrapbookUI/ScrapbookUI68K.bin
        data=build/retro68/68k/Release/example/ScrapbookUI/ASSETS.LRP
        ;;
    ScrapbookStandaloneFlow)
        target=LokaScrapbookStandaloneFlow68K_APPL
        bin=build/presentation/toolbox-68k-release/LokaScrapbookStandaloneFlow68K.bin
        data=build/presentation/toolbox-68k-release/ASSETS.LRP
        ;;
    HelloWorldStandaloneFlow)
        target=LokaHelloStandaloneFlow68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaHelloStandaloneFlow68K.bin
        ;;
    TutorialStandaloneFlow)
        target=LokaTutorialStandaloneFlow68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaTutorialStandaloneFlow68K.bin
        ;;
    MineSweeperStandaloneFlow)
        target=LokaMineStandaloneFlow68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaMineStandaloneFlow68K.bin
        ;;
    FloppyBirdStandaloneFlow)
        target=LokaFloppyStandaloneFlow68K_APPL
        preset=retro68-68k-standalone-release
        bin=build/retro68/68k/Standalone/Release/tests/toolbox/LokaFloppyStandaloneFlow68K.bin
        ;;
    HelloWorldScenarioLoop)
        target=LokaHelloScenarioLoop68K_APPL
        bin=build/retro68/68k/Release/tests/toolbox/LokaHelloScenarioLoop68K.bin
        ;;
    MineSweeperScenarioLoop)
        target=LokaMineScenarioLoop68K_APPL
        bin=build/retro68/68k/Release/tests/toolbox/LokaMineScenarioLoop68K.bin
        ;;
    *)
        echo "unknown SCSI app key: $key" >&2
        exit 2
        ;;
esac

build_app() {
    if [ "$key" = "ScrapbookStandaloneFlow" ]; then
        "$script_dir/toolbox-standalone-flow.sh" Stage
        return
    fi
    if [ "$key" = "SmirkyCard" ]; then
        "$script_dir/retro68-cmake.sh" --preset "$preset" -DLOKA_BUILD_SMIRKYCARD=ON
    else
        "$script_dir/retro68-cmake.sh" --preset "$preset"
    fi
    "$script_dir/retro68-cmake.sh" --build --preset "$preset" --target "$target"
}

if [ "$mode" = "--target" ]; then
    printf '%s\n' "$target"
elif [ "$mode" = "--build" ]; then
    build_app
else
    if [ "$mode" = "--build-and-prepare" ]; then
        build_app
    fi
    if [ -n "$data" ]; then
        exec "$script_dir/mame-dev-disk.sh" "$workspace_dir/$bin" "$workspace_dir/$data"
    else
        exec "$script_dir/mame-dev-disk.sh" "$workspace_dir/$bin"
    fi
fi
