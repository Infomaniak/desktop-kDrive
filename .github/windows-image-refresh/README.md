# Windows CI runner image refresh

`.github/workflows/update-windows-image.yml` refreshes the OpenStack image used by
`start-runners.yml` (`win_kdrive_runner_build_env`). Run it manually from the Actions tab
(**Refresh Windows CI runner image**).

## What it does

1. **Maintenance VM** (`img-refresh-<repository-id>-maint-<run>-<attempt>`) boots from the current image with
   `maintenance-userdata.ps1`. Its runner registers right away (unique label only, no default labels)
   but starts only after a SYSTEM startup task has installed every routine Windows update
   (no feature upgrades, drivers or previews), rebooting as needed.
2. **warm-cache** runs on that runner: it fails if Windows Update failed, then refreshes Conan with
   `build_dependencies.ps1 Release -CI -MakeRelease -CleanCache -Update` and schedules `seal.ps1`.
3. After the job ends, `seal.ps1` removes the runner state, autologon password and maintenance
   files, writes `C:\ProgramData\kDriveImageRefresh\refresh-info.json`, and runs
   `sysprep /generalize /oobe /shutdown` with the `sysprep_unattend` input.
4. **capture** waits for `SHUTOFF`, snapshots the VM and deletes it. A temporary Linux VM with a
   temporary volume downloads the snapshot, runs `qemu-img convert -c -O qcow2`, `qemu-img check` and
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
8. **cleanup** attempts to delete this run's VMs, volume, security group, snapshot, runner registrations
   and the candidate if it was not promoted. Hard cancellation can leave resources for a later sweep.
   With `keep_failed_validation`, a failed validation keeps its VM and candidate for inspection; the
   next run deletes them. If the runner never comes online, the boot job prints the VM status and
   serial console.

## Prerequisites

- Existing secrets: `PUBLIC_CLOUD_OS_*`, `RUNNER_ADMIN_PAT`, `QT_ACCOUNT_EMAIL`, `QT_ACCOUNT_PASSWORD`.
- Optional repository variables override the conversion VM defaults:
  - `IMAGE_REFRESH_LINUX_IMAGE`: `Ubuntu 26.04 LTS Resolute Raccoon` (cloud-init required).
  - `IMAGE_REFRESH_LINUX_FLAVOR`: `a8-ram16-disk20-perf1`.
  - `IMAGE_REFRESH_VOLUME_TYPE`: `CEPH_1_perf1`.
- The image must be image-backed (not boot-from-volume) and contain Cloudbase-Init with the unattend
  file given by `sysprep_unattend`.
- The OpenStack project needs quota for two Windows VMs, one Linux VM, the work volume
  (`work_volume_size_gb`, default 200 GB, must hold the raw snapshot and the qcow2) and the extra images.
- The conversion VM is reached over SSH from the GitHub-hosted runner; a per-run security group only
  allows port 22 from that runner's public IPv4. SSH trusts the host key on first use. Glance
  transfers use short-lived project-scoped tokens; a transfer exceeding the token lifetime will fail.

## First run

GitHub requires the workflow file on the default branch before it can be dispatched.
Before that, the branch can explicitly dispatch the existing `start-runners.yml` workflow
with `count=0` and `labels=refresh-production-image` to run the production refresh. This
bridge does not start normal runners and must be removed before merging.
Run once with `image_name` set to a copy of the production image (for example
`win_kdrive_runner_build_env_test`) and `promote` enabled to test the whole chain before
refreshing production. Set `promote` to false to validate without replacing the image; the
unpromoted candidate is then deleted.

## Troubleshooting and rollback

- Windows Update log: printed by the *warm-cache* job (`C:\ImageRefresh\refresh.log` on the
  maintenance VM, and `C:\ProgramData\kDriveImageRefresh\windows-update.log` after sealing).
- If *capture* times out waiting for `SHUTOFF`, Sysprep failed; check
  `C:\Windows\System32\Sysprep\Panther\setuperr.log` from the VM console.
- Leftovers of a hard-cancelled run are swept at the start of the next refresh, scoped to this
  repository's ID and, for candidates, the selected image name.
- Rollback: rename the new image (for example to `<image>_bad_<date>`), then rename the desired
  `<image>_old_*` image back to `<image>`:
  `openstack image set --name win_kdrive_runner_build_env <old-image-id>`.
- Retained `_old_` images are never deleted automatically; remove obsolete ones manually.

Run the local helper regression checks with `bash .github/windows-image-refresh/test-lib.sh`.
On Windows, run `powershell -File .github\windows-image-refresh\test-maintenance.ps1` to simulate
update selection and bounded failure handling without changing the machine.
Run `powershell -File .github\windows-image-refresh\test-seal.ps1` to check the sealing idle guard.
