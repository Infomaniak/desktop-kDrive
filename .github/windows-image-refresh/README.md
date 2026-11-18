<!--
Infomaniak kDrive - Desktop
Copyright (C) 2023-2026 Infomaniak Network SA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->

# Windows CI runner image refresh

`.github/workflows/update-windows-image.yml` refreshes the OpenStack image used by
`start-runners.yml` (`win_kdrive_runner_build_env`). Run it manually from the Actions tab
(**Refresh Windows CI runner image**).

It also runs every Sunday and Wednesday at **23:00 Europe/Zurich**, following daylight saving
time. GitHub only starts scheduled workflows once they are on the default branch. Manual runs
always perform the full refresh, even if Windows Update installs nothing.

## What it does

1. **Maintenance VM** (`img-refresh-<repository-id>-maint-<run>-<attempt>`) boots from the current image with
   `maintenance-userdata.ps1`. Its runner registers right away (unique label only, no default labels)
   but starts only after a SYSTEM startup task has installed every routine Windows update
   (no feature upgrades, drivers or previews), rebooting as needed.
2. **warm-cache** runs on that runner: it fails if Windows Update failed and records the original
   and destination Windows versions and the successfully installed update titles across reboots.
   On automatic runs with no installations, it skips Conan and sealing; capture, compression,
   validation and promotion are also skipped, and cleanup deletes the maintenance VM.
   Otherwise, it refreshes Conan with
   `build_dependencies.ps1 Release -CI -MakeRelease -CleanCache -Update` and schedules `seal.ps1`.
   Defender intelligence updates count as installations even when the Windows build is unchanged.
3. After the job ends, `seal.ps1` removes the runner state and autologon password, then restarts. At
   startup it waits for servicing to be idle and restarts again (up to 3 times) while a reboot is still
   pending; then it removes the maintenance files, writes
   `C:\ProgramData\kDriveImageRefresh\refresh-info.json`, and runs
   `sysprep /generalize /oobe /shutdown` with the `sysprep_unattend` input.
4. **capture** waits for `SHUTOFF`, snapshots the VM and deletes it. A temporary Linux VM with a
   temporary volume downloads the snapshot, prints its sealing and Sysprep logs (read-only), runs
   `qemu-img convert -c -O qcow2`, `qemu-img check` and
   `qemu-img compare`, then uploads a private `<image>_candidate_<run>_<attempt>` image with the
   current image's properties.
5. **boot-candidate** starts a fresh VM from the candidate with the normal runner bootstrap
   (`templates/userdata-template.ps1`), restricted to the dedicated label
   `img-refresh-<repository-id>-validate-<run>-<attempt>`.
6. **validate** must be picked by that runner. It checks the runner identity, Windows version,
   Sysprep evidence and that Conan reuses the warmed binaries. **watchdog** cancels the run if the
   job is not picked within 30 minutes.
7. **promote** (if the `promote` input is set) checks that the validation job succeeded on the
   dedicated runner and that the image name still resolves to the original image, then renames:
   - current image → `<image>_old_<old Windows version>_<UTC date>` (never deleted);
   - candidate → `<image>`.
   If `promote` is false, cleanup instead retains a successfully validated candidate as
   `<image>_validated_<Windows version>_<UTC date>_<run>_<attempt>`, leaving the current image unchanged.
8. **cleanup** attempts to delete this run's VMs, volume, security group, snapshot, runner registrations
   and any candidate that was not promoted or retained. Hard cancellation can leave resources for a later sweep.
   With `keep_failed_validation`, a failed validation keeps its VM and candidate for inspection; the
   next run deletes them. If the runner never comes online, the boot job prints the VM status and
   serial console.
9. **notify** posts the outcome to the existing kChat notification channel using
   `KCHAT_WEBHOOK_URL` and `infomaniak-build-tools/notify_kchat.ps1`, after cleanup. Successful
   refreshes list the original and destination Windows versions and every successfully installed
   update; runs with no updates report the current Windows version instead. Failed runs are
   explicitly marked as failed, including cleanup failures after a successful promotion.
   Cancelled runs do not send a notification.

## Prerequisites

- Existing secrets: `PUBLIC_CLOUD_OS_*`, `RUNNER_ADMIN_PAT`, `QT_ACCOUNT_EMAIL`, `QT_ACCOUNT_PASSWORD`,
  `KCHAT_WEBHOOK_URL`.
- The image must be image-backed (not boot-from-volume) and contain Cloudbase-Init with the unattend
  file given by `sysprep_unattend`.
- The OpenStack project needs quota for two Windows VMs, one Linux VM, the work volume
  (`work_volume_size_gb`, default 200 GB, must hold the raw snapshot and the qcow2) and the extra images.
- The conversion VM is reached over SSH from the GitHub-hosted runner; a per-run security group only
  allows port 22 from that runner's public IPv4. SSH trusts the host key on first use. Glance
  transfers use short-lived project-scoped tokens; a transfer exceeding the token lifetime will fail.

## Manual operation

Set `promote` to false to validate a refresh without replacing the image. The validated candidate is
kept under its dated, versioned name.

## Troubleshooting and rollback

- Windows Update log: printed by the *warm-cache* job (`C:\ImageRefresh\refresh.log` on the
  maintenance VM, and `C:\ProgramData\kDriveImageRefresh\windows-update.log` after sealing).
- If *capture* times out waiting for `SHUTOFF`, Sysprep failed; check
  `C:\Windows\System32\Sysprep\Panther\setuperr.log` from the VM console.
- Leftovers of a hard-cancelled run are swept at the start of the next refresh, scoped to this
  repository's ID and, for candidates, the selected image name.
- If retaining a validated candidate fails, inspect or rename it before the next refresh, which sweeps
  images still using the temporary candidate name.
- Rollback: rename the new image (for example to `<image>_bad_<date>`), then rename the desired
  `<image>_old_*` image back to `<image>`:
  `openstack image set --name win_kdrive_runner_build_env <old-image-id>`.
- Retained `_old_` and `_validated_` images are never deleted automatically; remove obsolete ones manually.
