#!/usr/bin/env bash
set -u
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
claim=$script_dir/claim.sh
tmp=$(mktemp -d) || exit 1
trap 'rm -rf -- "$tmp"' EXIT
export LOKA_CLAIMS_DIR=$tmp/claims LOKA_CLAIM_TTL_HOURS=24
failures=0
expect() {
    local name=$1 expected=$2 pattern=$3 output status
    shift 3
    output=$("$@" 2>&1); status=$?
    if [ "$status" -eq "$expected" ] && [[ $output == *"$pattern"* ]]; then
        printf 'PASS %s\n' "$name"
    else
        printf 'FAIL %s: exit=%s expected=%s output=%s\n' "$name" "$status" "$expected" "$output"
        failures=$((failures + 1))
    fi
}
same_contents() { [[ $(cat -- "$1") == "$(cat -- "$2")" ]]; }
expect 'empty list' 0 'no claims' "$claim" list
expect 'free check' 0 'free issue-877' "$claim" check issue-877
expect 'take' 0 'claimed issue-877' "$claim" take issue-877 --as alice --purpose '#877 release hang'
expect 'held check' 1 'held by alice' "$claim" check issue-877
expect 'second owner refused with purpose' 1 '#877 release hang' "$claim" take issue-877 --as bob
cp -r -- "$LOKA_CLAIMS_DIR/issue-877" "$tmp/before"
expect 'same owner retake' 0 'already yours' "$claim" take issue-877 --as alice --purpose changed
for field in owner purpose taken host; do
    expect "retake preserves $field" 0 '' same_contents "$tmp/before/$field" "$LOKA_CLAIMS_DIR/issue-877/$field"
done
expect 'foreign release refused' 1 'held by alice' "$claim" release issue-877 --as bob
expect 'fresh foreign force refused' 1 'held by alice' "$claim" release issue-877 --as bob --force
expect 'fresh own force refused' 1 'held by alice' "$claim" release issue-877 --as alice --force
expect 'owner release' 0 'released issue-877' "$claim" release issue-877 --as alice
expect 'released check' 0 'free issue-877' "$claim" check issue-877
expect 'stale setup' 0 'claimed vm-golden' "$claim" take vm-golden --as alice
printf '2000-01-01T00:00:00Z\n' > "$LOKA_CLAIMS_DIR/vm-golden/taken"
expect 'stale check' 0 'STALE' "$claim" check vm-golden
expect 'stale list' 0 'STALE' "$claim" list
expect 'stale take does not steal' 1 'held by alice' "$claim" take vm-golden --as bob
expect 'stale force release' 0 'released vm-golden' "$claim" release vm-golden --as bob --force
for bad in '../escape' 'UPPER' '-flag' 'a/b' 'a b' ''; do
    expect "bad key [$bad]" 2 'invalid key' "$claim" take "$bad" --as alice
done
expect 'bad owner' 2 'owner' "$claim" take valid --as ../alice
expect 'missing owner' 2 'owner' "$claim" take valid
expect 'invalid TTL' 2 'TTL' env LOKA_CLAIM_TTL_HOURS=no "$claim" list
expect 'zero TTL accepted' 0 'no claims' env LOKA_CLAIM_TTL_HOURS=0 "$claim" list
expect 'multiline purpose refused' 2 'single line' "$claim" take valid --as alice --purpose $'a\nb'
mkdir -- "$LOKA_CLAIMS_DIR/incomplete"
expect 'unpublished check held' 1 'INCOMPLETE' "$claim" check incomplete
expect 'unpublished list visible' 0 'INCOMPLETE' "$claim" list
expect 'unpublished take refused' 1 'INCOMPLETE' "$claim" take incomplete --as bob
expect 'unpublished force refused' 1 'INCOMPLETE' "$claim" release incomplete --as bob --force
printf 'owner=alice\n' > "$LOKA_CLAIMS_DIR/incomplete/owner"
expect 'partial same-owner take refused' 1 'INCOMPLETE' "$claim" take incomplete --as alice
expect 'partial owner release refused' 1 'INCOMPLETE' "$claim" release incomplete --as alice
expect 'release lock setup' 0 'claimed locked' "$claim" take locked --as alice
mkdir -- "$LOKA_CLAIMS_DIR/locked/.releasing"
expect 'interrupted release visible' 0 'RELEASING' "$claim" list
expect 'overlapping release refused' 1 'release busy' "$claim" release locked --as alice
expect 'retake while releasing refused' 1 'RELEASING' "$claim" take locked --as alice
rmdir -- "$LOKA_CLAIMS_DIR/locked/.releasing"
expect 'release after lock cleared' 0 'released locked' "$claim" release locked --as alice

pids=()
for i in {1..20}; do
    ( "$claim" take race --as "racer-$i" > "$tmp/race-$i.out" 2>&1
      echo "$?" > "$tmp/race-$i.status" ) &
    pids+=("$!")
done
for pid in "${pids[@]}"; do wait "$pid"; done
wins=0; losses=0
for i in {1..20}; do
    read -r status < "$tmp/race-$i.status"
    case "$status" in 0) wins=$((wins + 1));; 1) losses=$((losses + 1));; esac
done
if [ "$wins" -eq 1 ] && [ "$losses" -eq 19 ]; then
    echo 'PASS concurrent race (one winner, nineteen refusals)'
else
    echo "FAIL concurrent race: $wins winners, $losses refusals"
    failures=$((failures + 1))
fi
[ "$failures" -eq 0 ]
