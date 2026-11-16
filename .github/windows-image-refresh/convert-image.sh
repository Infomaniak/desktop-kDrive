#!/usr/bin/env bash
# Runs on the temporary Linux conversion VM (as root, over SSH) for the Windows image refresh workflow.
# Short-lived Keystone tokens are read from stdin and never written to disk.
#
#   convert-image.sh prepare <volume-id>              Format and mount the attached work volume on /mnt/work
#   convert-image.sh download <image-url> <bytes>     Download the snapshot data (token on stdin)
#   convert-image.sh convert                          Compress to qcow2, check and compare against the source
#   convert-image.sh upload <image-url>               Upload the qcow2 to a queued Glance image (token on stdin)
set -euo pipefail

WORK=/mnt/work
SRC="$WORK/snapshot.img"
OUT="$WORK/candidate.qcow2"

log() { echo "[$(date -u +%FT%TZ)] $*"; }
die() { echo "::error::$*" >&2; exit 1; }

read_token() {
    local token
    IFS= read -r token || true
    [[ -n "$token" ]] || die "No token provided on stdin"
    printf '%s' "$token"
}

available_bytes() { df --output=avail -B1 "$WORK" | tail -n1 | tr -d ' '; }

cmd_prepare() {
    local volume_id="$1"
    local root_source root_ancestors ancestor ancestor_device device devices filesystems mounts link
    # The Cinder volume ID (truncated to 20 characters by virtio-blk) is the disk serial. virtio-blk
    # exposes it as virtio-<serial>, virtio-scsi as scsi-0QEMU_QEMU_HARDDISK_<serial> (and variants).
    device=""
    for _ in {1..12}; do
        devices=""
        for link in /dev/disk/by-id/*"${volume_id:0:20}"*; do
            [[ -e "$link" && "$link" != *-part* ]] || continue
            devices+="$(readlink -f "$link")"$'\n'
        done
        devices="$(sort -u <<< "$devices" | sed '/^$/d')"
        if [[ -n "$devices" ]]; then
            [[ "$(wc -l <<< "$devices")" -eq 1 ]] || die "Work volume ${volume_id} matches several disks: ${devices//$'\n'/ }"
            device="$devices"
            [[ -b "$device" ]] && break
        fi
        sleep 10
    done
    [[ -b "$device" ]] || die "Work volume ${volume_id} not found under /dev/disk/by-id"

    [[ "$(lsblk -dn -o TYPE "$device")" == "disk" ]] || die "Work volume is not a disk: $device"
    root_source="$(findmnt -no SOURCE /)"
    [[ -b "$root_source" ]] || die "Cannot identify the root filesystem device: $root_source"
    root_ancestors="$(lsblk -s -l -n -p -o NAME "$root_source")" || die "Cannot inspect the root filesystem device"
    [[ -n "$root_ancestors" ]] || die "No root filesystem device ancestry found"
    while IFS= read -r ancestor; do
        ancestor_device="$(readlink -f "$ancestor")" || die "Cannot resolve root filesystem ancestor $ancestor"
        [[ "$ancestor_device" != "$device" ]] || die "Work volume is an ancestor of the root filesystem ($device)"
    done <<< "$root_ancestors"
    filesystems="$(lsblk -no FSTYPE "$device")" || die "Cannot inspect work volume filesystems"
    mounts="$(lsblk -no MOUNTPOINT "$device")" || die "Cannot inspect work volume mounts"
    [[ -z "$(tr -d '[:space:]' <<< "$filesystems")" ]] || die "Work volume $device is not empty"
    [[ -z "$(tr -d '[:space:]' <<< "$mounts")" ]] || die "Work volume $device is mounted"

    log "Formatting work volume $device"
    mkfs.ext4 -q -m 0 "$device"
    mkdir -p "$WORK"
    mount "$device" "$WORK"
    df -h "$WORK"
}

cmd_download() {
    local url="$1" expected_size="$2" token free
    token="$(read_token)"
    free="$(available_bytes)"
    (( free > expected_size + 10 * 1024 ** 3 )) || die "Not enough space to download ${expected_size} bytes (${free} available)"

    log "Downloading snapshot (${expected_size} bytes)"
    curl --fail --silent --show-error --retry 3 \
        -H "X-Auth-Token: ${token}" -o "$SRC" "$url"
    [[ "$(stat -c %s "$SRC")" == "$expected_size" ]] || die "Downloaded size does not match Glance size"
}

cmd_convert() {
    local format virtual_size free
    format="$(qemu-img info --output=json "$SRC" | jq -r '.format')"
    virtual_size="$(qemu-img info --output=json "$SRC" | jq -r '."virtual-size"')"
    [[ "$format" == "raw" || "$format" == "qcow2" ]] || die "Unexpected snapshot format: $format"

    free="$(available_bytes)"
    # Worst case: no compression gain, plus qcow2 metadata and headroom.
    (( free > virtual_size + 5 * 1024 ** 3 )) || die "Not enough space to convert ${virtual_size} bytes (${free} available)"

    log "Converting ${format} snapshot (${virtual_size} bytes virtual) to compressed qcow2"
    qemu-img convert -p -c -f "$format" -O qcow2 "$SRC" "$OUT"
    qemu-img check -f qcow2 "$OUT"
    log "Comparing guest-visible contents"
    qemu-img compare -f "$format" -F qcow2 "$SRC" "$OUT"
    rm -f "$SRC"

    echo "virtual_size=${virtual_size}"
    echo "size=$(stat -c %s "$OUT")"
    echo "md5=$(md5sum "$OUT" | cut -d' ' -f1)"
    echo "sha512=$(sha512sum "$OUT" | cut -d' ' -f1)"
}

cmd_upload() {
    local url="$1" token
    token="$(read_token)"
    log "Uploading candidate image ($(stat -c %s "$OUT") bytes)"
    curl --fail --silent --show-error -X PUT \
        -H "X-Auth-Token: ${token}" -H "Content-Type: application/octet-stream" \
        --upload-file "$OUT" "$url"
}

case "${1:-}" in
    prepare) cmd_prepare "$2" ;;
    download) cmd_download "$2" "$3" ;;
    convert) cmd_convert ;;
    upload) cmd_upload "$2" ;;
    *) die "Unknown command: ${1:-}" ;;
esac
