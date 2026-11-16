#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=.github/windows-image-refresh/lib.sh
source "$(dirname "$0")/lib.sh"

fail() { echo "FAIL: $*" >&2; exit 1; }

# Model image names, including API errors after a successful server-side rename.
os() {
    local action="$2" id name
    case "$action" in
        set)
            id="${!#}"
            name="$4"
            if [[ "$id" == source && "$name" == previous ]]; then
                [[ "$MODE" != first_fail ]] || return 1
                SOURCE_NAME="$name"
                [[ "$MODE" != first_applied ]] || return 1
            elif [[ "$id" == candidate ]]; then
                [[ "$MODE" != candidate_fail && "$MODE" != conflict && "$MODE" != rollback_fail ]] || return 1
                CANDIDATE_NAME="$name"
                [[ "$MODE" != candidate_applied ]] || return 1
            else
                [[ "$MODE" != rollback_fail ]] || return 1
                SOURCE_NAME="$name"
            fi
            ;;
        show)
            [[ "$3" == source ]] && printf '%s\n' "$SOURCE_NAME" || printf '%s\n' "$CANDIDATE_NAME"
            ;;
        list)
            [[ "$MODE" != conflict ]] || printf 'unexpected\n'
            [[ "$SOURCE_NAME" != current ]] || printf 'source\n'
            [[ "$CANDIDATE_NAME" != current ]] || printf 'candidate\n'
            return 0
            ;;
        *) fail "Unexpected operation: $*" ;;
    esac
}

check_promotion() (
    MODE="$1" SOURCE_NAME=current CANDIDATE_NAME=candidate-name
    if promote_image source candidate current previous; then
        [[ "$2" == success ]] || fail "$MODE unexpectedly succeeded"
    else
        [[ "$2" == failure ]] || fail "$MODE unexpectedly failed"
    fi
    [[ "$SOURCE_NAME" == "$3" ]] || fail "$MODE: original image is $SOURCE_NAME, expected $3"
    [[ "$CANDIDATE_NAME" == "$4" ]] || fail "$MODE: candidate image is $CANDIDATE_NAME, expected $4"
)

check_promotion success success previous current
check_promotion first_fail failure current candidate-name
check_promotion first_applied failure current candidate-name
check_promotion candidate_fail failure current candidate-name
check_promotion candidate_applied success previous current
check_promotion conflict failure previous candidate-name
check_promotion rollback_fail failure previous candidate-name

test_home="$(mktemp -d)"
trap 'rm -rf -- "$test_home"' EXIT
if command -v cygpath >/dev/null 2>&1; then
    HOME="$(cygpath -w "$test_home")"
else
    HOME="$test_home"
fi
export HOME
export OS_CLOUD=infomaniak
export CLOUD_AUTH_URL='https://cloud.example.invalid/v3'
export CLOUD_USERNAME='user: # name'
export CLOUD_PASSWORD='  pass: # "quoted" \ value  '
export CLOUD_PROJECT_ID=project
export CLOUD_USER_DOMAIN_NAME=default
export CLOUD_REGION_NAME=region
write_clouds_yaml
python3 - <<'PY'
import json
import os
from pathlib import Path

config = json.loads((Path(os.environ["HOME"]) / ".config/openstack/clouds.yaml").read_text())
auth = config["clouds"][os.environ["OS_CLOUD"]]["auth"]
assert auth["password"] == os.environ["CLOUD_PASSWORD"]
assert auth["username"] == os.environ["CLOUD_USERNAME"]
PY
echo "Image refresh helper tests passed."
