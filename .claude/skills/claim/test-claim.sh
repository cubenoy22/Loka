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
touch -d '2000-01-01 UTC' -- "$LOKA_CLAIMS_DIR/incomplete"
expect 'killed take becomes stale' 0 'STALE' "$claim" check incomplete
expect 'killed take force recovery' 0 'released incomplete' "$claim" release incomplete --as bob --force
expect 'recovered key free' 0 'free incomplete' "$claim" check incomplete
mkdir -- "$LOKA_CLAIMS_DIR/bad-time"
printf 'broken\n' > "$LOKA_CLAIMS_DIR/bad-time/taken"
touch -d '2000-01-01 UTC' -- "$LOKA_CLAIMS_DIR/bad-time"
expect 'unparsable timestamp recovery' 0 'released bad-time' "$claim" release bad-time --as bob --force
mkdir -- "$LOKA_CLAIMS_DIR/.gone.leftover.123"
expect 'fresh gone hidden' 0 'no claims' "$claim" list
touch -d '2000-01-01 UTC' -- "$LOKA_CLAIMS_DIR/.gone.leftover.123"
expect 'old gone warning' 0 "WARNING stale release leftover: $LOKA_CLAIMS_DIR/.gone.leftover.123" "$claim" list
expect 'gone does not block key' 0 'claimed leftover' "$claim" take leftover --as alice

expect 'ABA setup' 0 'claimed aba' "$claim" take aba --as alice
swap_owner='printf "owner=bob\n" > "$LOKA_CLAIMS_DIR/aba/owner"'
expect 'ABA owner change refused' 1 'release raced' env LOKA_CLAIM_TEST_BEFORE_MV="$swap_owner" "$claim" release aba --as alice
expect 'ABA new owner survives' 1 'held by bob' "$claim" check aba
swap_time='printf "2001-01-01T00:00:00Z\n" > "$LOKA_CLAIMS_DIR/aba/taken"'
expect 'ABA timestamp change refused' 1 'release raced' env LOKA_CLAIM_TEST_BEFORE_MV="$swap_time" "$claim" release aba --as bob
expect 'ABA new timestamp survives' 0 '2001-01-01T00:00:00Z' cat "$LOKA_CLAIMS_DIR/aba/taken"
expect 'ABA claim remains releasable' 0 'released aba' "$claim" release aba --as bob
expect 'rename race setup' 0 'claimed vanished' "$claim" take vanished --as alice
vanish='rm -rf -- "$LOKA_CLAIMS_DIR/vanished"'
expect 'rename failure reports race' 1 'release raced' env LOKA_CLAIM_TEST_BEFORE_MV="$vanish" "$claim" release vanished --as alice

# Simulate a third session taking the key between rename and ABA restoration.
mkdir -- "$tmp/bin"
cat > "$tmp/bin/mv" <<'WRAPPER'
#!/usr/bin/env bash
"$CLAIM_TEST_REAL_MV" "$@" || exit "$?"
if [ "$#" -eq 4 ] && [ "$2" = -- ]; then
    "$CLAIM_TEST_TOOL" take conflict --as charlie > /dev/null || exit 1
fi
WRAPPER
chmod +x "$tmp/bin/mv"
expect 'conflict setup' 0 'claimed conflict' "$claim" take conflict --as alice
swap_conflict='printf "owner=bob\n" > "$LOKA_CLAIMS_DIR/conflict/owner"'
expect 'ABA conflict reports preserved path' 1 "conflict preserved at $LOKA_CLAIMS_DIR/.gone.conflict." env \
    PATH="$tmp/bin:$PATH" CLAIM_TEST_REAL_MV="$(command -v mv)" CLAIM_TEST_TOOL="$claim" \
    LOKA_CLAIM_TEST_BEFORE_MV="$swap_conflict" "$claim" release conflict --as alice
expect 'ABA conflict keeps third claimant' 1 'held by charlie' "$claim" check conflict
preserved_conflict() {
    local dirs=("$LOKA_CLAIMS_DIR"/.gone.conflict.*)
    [ "${#dirs[@]}" -eq 1 ] && [ "$(cat -- "${dirs[0]}/owner")" = 'owner=bob' ]
}
expect 'ABA conflict keeps displaced claimant' 0 '' preserved_conflict

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
