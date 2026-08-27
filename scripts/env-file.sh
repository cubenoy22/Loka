#!/usr/bin/env bash

# Safe dotenv mechanics shared by host-side Bash entry points. Values support
# simple quotes, HOME expansion, CRLF files, and escaped spaces without
# evaluating command substitutions or other shell code.

loka_env_trim_whitespace() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

loka_env_expand_value() {
  local value="$1"

  if [ "$value" = "~" ]; then
    value="$HOME"
  elif [[ "$value" == \~/* ]]; then
    value="$HOME/${value:2}"
  fi
  value="${value//\$\{HOME\}/$HOME}"
  value="${value//\$HOME/$HOME}"
  value="${value//\\ / }"
  printf '%s' "$value"
}

loka_import_environment_file() {
  local path="$1"
  local allowed_names="${2:-}"
  local preserve_existing="${3:-0}"
  local line=""
  local name=""
  local value=""

  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
    line="$(loka_env_trim_whitespace "$line")"
    if [ -z "$line" ] || [[ "$line" == \#* ]]; then
      continue
    fi
    if [[ ! "$line" =~ ^([A-Za-z_][A-Za-z0-9_]*)=(.*)$ ]]; then
      echo "Error: invalid environment line in $path" >&2
      return 1
    fi

    name="${BASH_REMATCH[1]}"
    if [ -n "$allowed_names" ]; then
      case " $allowed_names " in
        *" $name "*) ;;
        *)
          echo "Error: unsupported setting '$name' in $path" >&2
          return 1
          ;;
      esac
    fi

    if [ "$preserve_existing" -eq 1 ] && [ "${!name+x}" = x ]; then
      continue
    fi

    value="$(loka_env_trim_whitespace "${BASH_REMATCH[2]}")"
    if [[ "$value" == \"*\" ]] || [[ "$value" == \'*\' ]]; then
      value="${value:1:${#value}-2}"
    fi
    value="$(loka_env_expand_value "$value")"
    printf -v "$name" '%s' "$value"
    export "$name"
  done < "$path"
}
