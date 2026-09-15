#!/usr/bin/env bash

# Prepare MAME's persistent, writable copy of a boot-disk template.
loka_prepare_boot_copy() {
  local template="$1" boot="$2" template_path template_identity target template_sha source old_path reason
  if [ ! -f "$template" ]; then
    echo "boot hard disk template not found: $template" >&2
    return 1
  fi
  mkdir -p "$(dirname "$boot")"
  if [ -e "$boot" ] && [ "$boot" -ef "$template" ]; then
    echo "MAME_BOOT_HDA resolves to the boot template itself: $boot" >&2
    return 1
  fi
  template_path="$(cd "$(dirname "$template")" && pwd -P)/$(basename "$template")"
  while [ -L "$template_path" ]; do
    target="$(readlink "$template_path")"
    case "$target" in
      /*) template_path="$target" ;;
      *) template_path="$(dirname "$template_path")/$target" ;;
    esac
    template_path="$(cd "$(dirname "$template_path")" && pwd -P)/$(basename "$template_path")"
  done
  template_identity="$template_path"
  # MAME launch delegates from WSL to Windows PowerShell. Use the Windows
  # spelling for DrvFS templates so both sides publish the same provenance;
  # otherwise Start mistakes /mnt/c/... and C:\... for different templates
  # and replaces the just-staged disk with the pristine source image.
  if [ -n "${WSL_INTEROP:-}" ] && command -v wslpath >/dev/null 2>&1 \
    && [[ "$template_path" =~ ^/mnt/[A-Za-z]/ ]]; then
    template_identity="$(wslpath -w "$template_path")"
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    template_sha="$(sha256sum < "$template_path" | cut -d' ' -f1)"
  else
    template_sha="$(shasum -a 256 < "$template_path" | cut -d' ' -f1)"
  fi
  source="$boot.source"
  old_path=""
  if [ -f "$source" ]; then { IFS= read -r old_path || true; } < "$source"; fi
  if [ -e "$boot.previous" ] || [ -e "$source.previous" ] ||
     [ -d "$boot" ] || [ -d "$source" ]; then
    echo 'boot copy: refresh failed (destination or recovery path needs attention)' >&2
    return 1
  fi
  reason=""
  if [ ! -f "$boot" ]; then
    reason="copy missing"
  elif [ ! -f "$source" ]; then
    reason="source missing"
  elif ! printf '%s\n%s\n' "$template_identity" "$template_sha" | cmp -s - "$source"; then
    reason="template changed: $old_path → $template_identity"
  fi
  if [ -n "$reason" ]; then
    (
      local phase=staging
      finish_boot_copy() {
        local status=$?
        trap - EXIT
        if [ "$status" -ne 0 ]; then
          if [ -e "$boot.previous" ]; then
            mv -f "$boot.previous" "$boot" || { echo 'boot copy: refresh failed (rollback failed; recovery files retained)' >&2; exit 1; }
          elif [ "$phase" = installed ] || [ "$phase" = published ]; then
            rm -f "$boot" || exit 1
          fi
          if [ "$phase" = published ]; then
            if [ -f "$source.previous" ]; then mv -f "$source.previous" "$source" || exit 1; else rm -f "$source" || exit 1; fi
          fi
          if [ "$phase" = staging ]; then echo 'boot copy: refresh failed (previous copy unchanged)' >&2; else echo 'boot copy: refresh failed (previous copy restored)' >&2; fi
        fi
        rm -f "$boot.partial" "$source.partial" "$source.previous"
        exit "$status"
      }
      trap finish_boot_copy EXIT
      printf '%s\n%s\n' "$template_identity" "$template_sha" > "$source.partial"
      cp -f "$template_path" "$boot.partial"
      if [ -f "$source" ]; then cp -f "$source" "$source.previous"; fi
      if [ -e "$boot" ]; then mv -f "$boot" "$boot.previous"; fi
      phase=backed-up
      mv -f "$boot.partial" "$boot"
      phase=installed
      mv -f "$source.partial" "$source"
      phase=published
      rm -f "$boot.previous"
      phase=complete
    )
    printf 'boot copy: refreshed (%s)\n' "$reason"
  else
    echo 'boot copy: reused (same template)'
  fi
  if ! chmod u+w "$boot"; then
    echo "could not make the boot hard disk copy writable: $boot" >&2
    return 1
  fi
}
