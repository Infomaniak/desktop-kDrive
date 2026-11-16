#!/usr/bin/env bash
# Helpers sourced by .github/workflows/update-windows-image.yml steps.
# Requires: OS_CLOUD, REPO and, for GitHub runner helpers, GH_TOKEN.

log() { echo "[$(date -u +%FT%TZ)] $*"; }
die() { echo "::error::$*" >&2; exit 1; }

write_clouds_yaml() {
    mkdir -p ~/.config/openstack
    # JSON is valid YAML and safely quotes credentials containing YAML metacharacters.
    (umask 077; python3 - <<'PY'
import json
import os
from pathlib import Path

env = os.environ
cloud = {
    "auth": {
        "auth_url": env["CLOUD_AUTH_URL"],
        "username": env["CLOUD_USERNAME"],
        "password": env["CLOUD_PASSWORD"],
        "project_id": env["CLOUD_PROJECT_ID"],
        "user_domain_name": env["CLOUD_USER_DOMAIN_NAME"],
    },
    "region_name": env["CLOUD_REGION_NAME"],
    "interface": "public",
    "identity_api_version": 3,
}
with (Path(env["HOME"]) / ".config/openstack/clouds.yaml").open("w") as config:
    json.dump({"clouds": {env["OS_CLOUD"]: cloud}}, config)
PY
    )
    chmod 600 ~/.config/openstack/clouds.yaml
}

os() { openstack --os-cloud "$OS_CLOUD" "$@"; }

# Prints a strong random Windows administrator password.
random_admin_password() {
    local raw
    raw="$(openssl rand -base64 48 | tr -dc 'A-Za-z0-9')"
    [[ ${#raw} -ge 15 ]] || die "Password generation failed"
    printf '%s' "${raw:0:15}Aa1!"
}

mint_registration_token() {
    gh api -X POST "/repos/${REPO}/actions/runners/registration-token" --jq .token
}

# Prints "<id> <status> <busy>" for the runner registered with this exact name, or nothing.
runner_info() {
    gh api --paginate "/repos/${REPO}/actions/runners?per_page=100" \
        --jq ".runners[] | select(.name == \"$1\") | \"\(.id) \(.status) \(.busy)\""
}

delete_runner() {
    local id
    id="$(runner_info "$1" | awk '{print $1}')" || return 1
    if [[ -n "$id" ]]; then
        log "Deleting GitHub runner $1 (id=$id)"
        gh api -X DELETE "/repos/${REPO}/actions/runners/${id}" || return 1
    fi
}

server_status() { os server show "$1" -f value -c status; }

# wait_server_status <server-id> <wanted-status> <timeout-minutes>
wait_server_status() {
    local id="$1" wanted="$2" deadline status
    deadline=$(( $(date +%s) + $3 * 60 ))
    while true; do
        status="$(server_status "$id")"
        [[ "$status" == "$wanted" ]] && return 0
        [[ "$status" == "ERROR" ]] && die "Server $id is in ERROR state"
        (( $(date +%s) < deadline )) || die "Timeout waiting for server $id to be $wanted (currently $status)"
        sleep 30
    done
}

# wait_runner_online <runner-name> <server-id> <timeout-minutes>
wait_runner_online() {
    local name="$1" server="$2" deadline info
    deadline=$(( $(date +%s) + $3 * 60 ))
    while true; do
        info="$(runner_info "$name")"
        if [[ "$(awk '{print $2}' <<<"$info")" == "online" ]]; then
            log "Runner $name is online"
            return 0
        fi
        [[ "$(server_status "$server")" == "ERROR" ]] && die "Server $server is in ERROR state"
        (( $(date +%s) < deadline )) || die "Timeout waiting for runner $name to come online (${info:-not registered})"
        log "Waiting for runner $name (${info:-not registered yet})"
        sleep 60
    done
}

# wait_image_active <image-id> <timeout-minutes>
wait_image_active() {
    local id="$1" deadline status
    deadline=$(( $(date +%s) + $2 * 60 ))
    while true; do
        status="$(os image show "$id" -f value -c status)"
        [[ "$status" == "active" ]] && return 0
        [[ "$status" =~ ^(killed|deleted|deactivated)$ ]] && die "Image $id is $status"
        (( $(date +%s) < deadline )) || die "Timeout waiting for image $id to be active (currently $status)"
        sleep 30
    done
}

# Public image API endpoint without version suffix.
glance_endpoint() {
    python3 - <<'PY'
import os
import openstack
conn = openstack.connect(cloud=os.environ["OS_CLOUD"])
url = conn.session.get_endpoint(service_type="image", interface="public").rstrip("/")
print(url[:-3] if url.endswith("/v2") else url)
PY
}

# Exact name of the image ID resolved for a name; fails unless exactly one image has this name.
resolve_unique_image() {
    local ids
    ids="$(os image list --name "$1" -f value -c ID)"
    [[ -n "$ids" ]] || die "No image named $1"
    [[ "$(wc -l <<<"$ids")" -eq 1 ]] || die "Several images are named $1: $(tr '\n' ' ' <<<"$ids")"
    printf '%s' "$ids"
}

# Restore only after checking the names: a failed API call can still have taken effect.
restore_original_image() {
    local source="$1" name="$2" old_name="$3" source_name existing
    source_name="$(os image show "$source" -f value -c name)" || {
        echo "::error::The original image state is unknown; manual recovery is required." >&2
        return 1
    }
    [[ "$source_name" == "$name" ]] && return 0
    existing="$(os image list --name "$name" -f value -c ID)" || {
        echo "::error::Could not verify the production image name; manual recovery is required." >&2
        return 1
    }
    if [[ "$source_name" != "$old_name" ]] || [[ -n "$existing" ]]; then
        echo "::error::Image names changed during promotion; manual recovery is required." >&2
        return 1
    fi
    if os image set --name "$name" "$source"; then
        echo "::error::Promotion failed; the original image was restored." >&2
    else
        echo "::error::Promotion and rollback failed; restore image $source manually." >&2
    fi
    return 1
}

promote_image() {
    local source="$1" candidate="$2" name="$3" old_name="$4" candidate_name
    if ! os image set --name "$old_name" "$source"; then
        restore_original_image "$source" "$name" "$old_name"
        return 1
    fi
    if os image set --name "$name" "$candidate"; then
        return 0
    fi
    candidate_name="$(os image show "$candidate" -f value -c name)" || {
        echo "::error::Candidate rename failed and its state is unknown; inspect images before recovery." >&2
        return 1
    }
    if [[ "$candidate_name" == "$name" ]]; then
        log "Candidate rename succeeded despite the API error."
        return 0
    fi
    restore_original_image "$source" "$name" "$old_name"
    return 1
}
