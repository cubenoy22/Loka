#!/usr/bin/env bash
# Machine-local courtesy claims; GNU coreutils and Bash 5.
set -u
export LC_ALL=C
umask 077

usage() {
    echo 'usage: claim.sh take KEY --as OWNER [--purpose TEXT] | release KEY --as OWNER [--force] | check KEY | list' >&2
    exit 2
}
valid_name() { [[ $1 =~ ^[a-z0-9][a-z0-9._-]*$ ]]; }
error() { echo "$*" >&2; exit 2; }

command=${1:-}
[ "$#" -gt 0 ] || usage
shift
key= owner= purpose= force=0
case "$command" in
    take|release|check) [ "$#" -gt 0 ] || usage; key=$1; shift
        valid_name "$key" || error 'invalid key' ;;
    list) ;;
    *) usage ;;
esac
while [ "$#" -gt 0 ]; do
    case "$1" in
        --as) [[ $command == take || $command == release ]] || usage
            [ "$#" -ge 2 ] || usage; owner=$2; shift 2 ;;
        --purpose) [ "$command" = take ] || usage
            [ "$#" -ge 2 ] || usage; purpose=$2; shift 2 ;;
        --force) [ "$command" = release ] || usage; force=1; shift ;;
        *) usage ;;
    esac
done
if [[ $command == take || $command == release ]]; then
    valid_name "$owner" || error 'invalid or missing owner'
fi
# Keep every record and display on one line.
[[ $purpose != *[$'\r\n']* ]] || error 'purpose must be a single line'
ttl=${LOKA_CLAIM_TTL_HOURS:-24}
[[ $ttl =~ ^[0-9]{1,8}$ ]] || error 'TTL must be a nonnegative integer (at most 8 digits)'
ttl_seconds=$((10#$ttl * 3600))
root=${LOKA_CLAIMS_DIR:-$HOME/.loka-claims}
mkdir -p -- "$root" || error 'cannot create claims directory'
path=$root/$key

# The timestamp is published last; interrupted publication ages by directory mtime.
read_claim() {
    holder=unknown; detail=; age=unknown; marker=; stale=0
    record=; stamp=
    local seconds elapsed
    [ -d "$path" ] && [ ! -L "$path" ] || { marker=INCOMPLETE; return 0; }
    record=$(cat -- "$path/owner" 2>/dev/null) || marker=INCOMPLETE
    if [[ $record == owner=* ]] && valid_name "${record#owner=}"; then
        holder=${record#owner=}
    else
        marker=INCOMPLETE
    fi
    detail=$(cat -- "$path/purpose" 2>/dev/null) || marker=INCOMPLETE
    detail=${detail//$'\n'/ }; detail=${detail//$'\r'/ }
    [ -s "$path/host" ] || marker=INCOMPLETE
    stamp=$(cat -- "$path/taken" 2>/dev/null) || marker=INCOMPLETE
    seconds=
    if [[ $stamp =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]]; then
        seconds=$(date -u -d "$stamp" +%s 2>/dev/null) || seconds=
    fi
    if [ -z "$seconds" ]; then
        marker=INCOMPLETE
        seconds=$(stat -c %Y -- "$path" 2>/dev/null) || return 0
    fi
    elapsed=$(($(date -u +%s) - seconds))
    [ "$elapsed" -ge 0 ] || { marker=INCOMPLETE; return 0; }
    age="$((elapsed / 3600))h$((elapsed % 3600 / 60))m"
    if [ "$elapsed" -gt "$ttl_seconds" ]; then
        stale=1; marker="${marker:+$marker }STALE"
    fi
}
report() { printf '%s held by %s for %s: %s%s\n' "$key" "$holder" "$age" "$detail" "${marker:+ [$marker]}"; }

case "$command" in
    take)
        if mkdir -- "$path" 2>/dev/null; then
            # Failure leaves a visible INCOMPLETE claim for inspection.
            { printf 'owner=%s\n' "$owner" > "$path/owner" &&
              printf '%s\n' "$purpose" > "$path/purpose" &&
              uname -n > "$path/host" &&
              date -u +%Y-%m-%dT%H:%M:%SZ > "$path/taken"; } || error "incomplete claim: $key"
            printf 'claimed %s\n' "$key"
        else
            [[ -e $path || -L $path ]] || error "cannot create claim: $key"
            read_claim
            if [ "$holder" = "$owner" ] && [[ $marker != *INCOMPLETE* ]]; then
                printf 'already yours: %s\n' "$key"
            else report; exit 1; fi
        fi ;;
    check)
        if [[ ! -e $path && ! -L $path ]]; then echo "free $key"; exit 0; fi
        read_claim; report
        [ "$stale" -eq 1 ] ;;
    list)
        count=0
        for path in "$root"/*; do
            [[ -e $path || -L $path ]] || continue
            key=${path##*/}
            valid_name "$key" || continue
            read_claim; report; count=$((count + 1))
        done
        [ "$count" -ne 0 ] || echo 'no claims'
        for gone in "$root"/.gone.*; do
            [ -d "$gone" ] && [ ! -L "$gone" ] || continue
            seconds=$(stat -c %Y -- "$gone" 2>/dev/null) || continue
            if [ "$(($(date -u +%s) - seconds))" -gt "$ttl_seconds" ]; then
                printf 'WARNING stale release leftover: %s (inspect and remove by hand)\n' "$gone"
            fi
        done ;;
    release)
        [[ -e $path || -L $path ]] || { echo "free $key"; exit 0; }
        [ -d "$path" ] && [ ! -L "$path" ] || error "invalid claim directory: $key"
        read_claim
        if { [ "$force" -eq 1 ] && [ "$stale" -eq 0 ]; } ||
           { [ "$force" -eq 0 ] && { [ "$holder" != "$owner" ] || [[ $marker == *INCOMPLETE* ]]; }; }; then
            report; exit 1
        fi
        validated_owner=$record; validated_taken=$stamp
        gone=$root/.gone.$key.$$
        # Test-only hook for deterministic replacement between validation and mv.
        if [ -n "${LOKA_CLAIM_TEST_BEFORE_MV:-}" ]; then
            bash -c "$LOKA_CLAIM_TEST_BEFORE_MV" || error 'before-mv test hook failed'
        fi
        # PID reuse may leave this name behind; never overwrite its contents.
        if [[ -e $gone || -L $gone ]] || ! mv -T -- "$path" "$gone"; then
            echo "release raced: $key"; exit 1
        fi
        moved_owner=$(cat -- "$gone/owner" 2>/dev/null) || moved_owner=
        moved_taken=$(cat -- "$gone/taken" 2>/dev/null) || moved_taken=
        if [ "$moved_owner" != "$validated_owner" ] || [ "$moved_taken" != "$validated_taken" ]; then
            # -n also protects a take that wins after a free-path observation.
            if mv -T -n -- "$gone" "$path" && [[ ! -e $gone && ! -L $gone ]]; then
                echo "release raced: $key (replacement restored)"
            else
                echo "release raced: $key; conflict preserved at $gone"
            fi
            exit 1
        fi
        rm -rf -- "$gone" || error "cannot finish release: $gone"
        echo "released $key" ;;
esac
