#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "usage: $0 macos <key>" >&2
    exit 2
}

[ "$#" -eq 2 ] || usage
platform=$1
key=$2
[ "$platform" = "macos" ] || usage

preset=macos-debug
target=
configure_args=()

case "$key" in
    AllLoops)
        for loop in ScrapbookStandaloneLoop HelloWorldStandaloneLoop TutorialStandaloneLoop MineSweeperStandaloneLoop FloppyBirdStandaloneLoop HelloWorldScenarioLoop MineSweeperScenarioLoop; do
            bash "${BASH_SOURCE[0]}" macos "$loop"
        done
        exit 0
        ;;
    All) ;;
    HelloWorld) target=LokaHelloMacOS ;;
    MineSweeper) target=LokaMineMacOS ;;
    SimpleViewer) target=LokaSimpleViewerMacOS ;;
    ScrapbookUI) target=ScrapbookUIMacOS ;;
    FloppyBird) target=LokaFloppyBirdMacOS ;;
    SmirkBench) target=LokaSmirkBenchMacOS ;;
    LazyList) target=LokaLazyListMacOS ;;
    Tutorial) target=LokaTutorialMacOS ;;
    SmirkyCard)
        target=LokaSmirkyCardMacOS
        configure_args=(-DLOKA_BUILD_SMIRKYCARD=ON)
        ;;
    Tests)
        preset=macos-tests
        target=LokaTestsMacOS
        ;;
    HelloWorldScenarioLoop) target=LokaHelloWorldScenarioLoopMacOS ;;
    ScrapbookStandaloneLoop) preset=macos-standalone-release; target=LokaScrapbookStandaloneLoopMacOS ;;
    HelloWorldStandaloneLoop) preset=macos-standalone-release; target=LokaHelloWorldStandaloneLoopMacOS ;;
    TutorialStandaloneLoop) preset=macos-standalone-release; target=LokaTutorialStandaloneLoopMacOS ;;
    MineSweeperStandaloneLoop) preset=macos-standalone-release; target=LokaMineSweeperStandaloneLoopMacOS ;;
    FloppyBirdStandaloneLoop) preset=macos-standalone-release; target=LokaFloppyBirdStandaloneLoopMacOS ;;
    MineSweeperScenarioLoop) target=LokaMineSweeperScenarioLoopMacOS ;;
    *)
        echo "unknown macOS build key: $key" >&2
        exit 2
        ;;
esac

cmake --preset "$preset" "${configure_args[@]}"
if [ "$preset" = "macos-standalone-release" ]; then
    preset=macos-standalone-loop-release
fi
if [ -n "$target" ]; then
    exec cmake --build --preset "$preset" --target "$target"
fi
exec cmake --build --preset "$preset"
