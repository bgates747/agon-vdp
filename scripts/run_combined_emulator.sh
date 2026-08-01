#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/run_combined_emulator.sh [--headless-smoke] [--profile PATH] [Fab options...]

Launch the isolated combined Pingo + Wolf3DOrig emulator profile. The default
interactive autoexec runs Earth Party Flat; press Escape to advance to the
playable Wolf3DOrig fixture, then Escape again to return to MOS.

--headless-smoke uses a disposable SD view. It runs the finite 36-frame Pingo
Cube fixture, then a combined-owned Wolf opcode-0 dispatch-and-exit fixture.
It requires diagnostic evidence from both VDP extensions.

Environment:
  COMBINED_EMULATOR_SMOKE_TIMEOUT  Headless timeout in seconds (default: 60)
  COMBINED_EMULATOR_SMOKE_LOG      Optional path retaining the full smoke log
  SDL_VIDEODRIVER                  Interactive driver (default: wayland)
  SDL_AUDIODRIVER                  Interactive audio driver; unset lets SDL choose
EOF
}

default_profile="${HOME}/Agon/mystuff/agon-dev-env/emulators/wolf-pingo-v2.16"
profile="${default_profile}"
headless=0
smoke_log_destination=""

validate_smoke_log_destination() {
    local requested="$1"
    local requested_parent requested_name resolved_parent resolved_destination

    if [[ "${requested}" == *$'\n'* || "${requested}" == *$'\r'* ]]; then
        printf '%s\n' \
            'COMBINED_EMULATOR_SMOKE_LOG must not contain a newline.' >&2
        return 1
    fi
    if [[ "${requested}" == */ ]]; then
        printf 'COMBINED_EMULATOR_SMOKE_LOG is not a file path: %s\n' \
            "${requested}" >&2
        return 1
    fi

    requested_parent="$(dirname -- "${requested}")"
    requested_name="$(basename -- "${requested}")"
    if [[ "${requested_name}" == . || "${requested_name}" == .. ]]; then
        printf 'COMBINED_EMULATOR_SMOKE_LOG is not a valid file name: %s\n' \
            "${requested}" >&2
        return 1
    fi
    if [[ -L "${requested}" || -e "${requested}" ]]; then
        printf 'Refusing existing smoke-log destination: %s\n' \
            "${requested}" >&2
        return 1
    fi
    if [[ -L "${requested_parent}" ]]; then
        printf 'Refusing symlinked smoke-log parent: %s\n' \
            "${requested_parent}" >&2
        return 1
    fi
    if [[ ! -d "${requested_parent}" ]]; then
        printf 'Smoke-log parent must be an existing directory: %s\n' \
            "${requested_parent}" >&2
        return 1
    fi
    if [[ ! -w "${requested_parent}" ]]; then
        printf 'Smoke-log parent is not writable: %s\n' \
            "${requested_parent}" >&2
        return 1
    fi
    if ! resolved_parent="$(readlink -f -- "${requested_parent}")"; then
        printf 'Could not resolve smoke-log parent: %s\n' \
            "${requested_parent}" >&2
        return 1
    fi
    resolved_destination="${resolved_parent}/${requested_name}"
    if [[ -L "${resolved_destination}" || -e "${resolved_destination}" ]]; then
        printf 'Refusing existing resolved smoke-log destination: %s\n' \
            "${resolved_destination}" >&2
        return 1
    fi
    smoke_log_destination="${resolved_destination}"
}

while (($#)); do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --headless-smoke)
            headless=1
            shift
            ;;
        --profile)
            if (($# < 2)); then
                printf '%s\n' '--profile requires a path' >&2
                exit 2
            fi
            profile="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac
done

if ((headless)) && [[ -n "${COMBINED_EMULATOR_SMOKE_LOG:-}" ]]; then
    validate_smoke_log_destination "${COMBINED_EMULATOR_SMOKE_LOG}"
fi

if [[ ! -d "${profile}" ]]; then
    printf 'Combined-emulator profile is missing: %s\n' "${profile}" >&2
    exit 1
fi
for link in fab-agon-emulator vdp_combined.so mos_console8.bin; do
    if [[ ! -e "${profile}/${link}" ]]; then
        printf 'Combined-emulator profile link is missing or broken: %s\n' \
            "${profile}/${link}" >&2
        exit 1
    fi
done
if [[ ! -d "${profile}/sdcard" ]]; then
    printf 'Combined-emulator SD directory is missing: %s\n' \
        "${profile}/sdcard" >&2
    exit 1
fi

profile="$(readlink -f -- "${profile}")"
fab_bin="$(readlink -f -- "${profile}/fab-agon-emulator")"
vdp_module="$(readlink -f -- "${profile}/vdp_combined.so")"
mos_bin="$(readlink -f -- "${profile}/mos_console8.bin")"
mos_map="${mos_bin%.bin}.map"
profile_sd="$(readlink -f -- "${profile}/sdcard")"
profile_checksums="${profile}/profile-checksums.sha256"

for required in "${fab_bin}" "${vdp_module}" "${mos_bin}" "${mos_map}"; do
    if [[ ! -f "${required}" ]]; then
        printf 'Required combined-emulator artifact is missing: %s\n' "${required}" >&2
        exit 1
    fi
done
if [[ ! -d "${profile_sd}" ]]; then
    printf 'Required combined-emulator SD directory is missing: %s\n' "${profile_sd}" >&2
    exit 1
fi
if [[ ! -f "${profile_checksums}" ]]; then
    printf 'Combined-emulator checksum manifest is missing: %s\n' "${profile_checksums}" >&2
    exit 1
fi
if [[ -L "${profile_checksums}" ]]; then
    printf 'Refusing symlinked combined-emulator checksum manifest: %s\n' \
        "${profile_checksums}" >&2
    exit 1
fi

run_root="$(mktemp -d "${TMPDIR:-/tmp}/fab-wolf-pingo.XXXXXX")"
keep_run_root=0
cleanup() {
    if ((keep_run_root)); then
        printf 'Retained failed combined-emulator run: %s\n' "${run_root}" >&2
    else
        rm -rf -- "${run_root}"
    fi
}
trap cleanup EXIT

manifest_snapshot="${run_root}/profile-checksums.sha256"
manifest_hash_before="$(sha256sum -- "${profile_checksums}")"
manifest_hash_before="${manifest_hash_before%% *}"
if ! sha256sum --status --check "${profile_checksums}"; then
    printf '%s\n' \
        'Combined-emulator artifacts drifted after setup; rerun setup only after reviewing the new identities.' \
        >&2
    exit 1
fi
if ! cp --preserve=mode,timestamps -- \
    "${profile_checksums}" "${manifest_snapshot}"; then
    keep_run_root=1
    printf 'Could not snapshot checksum manifest; run retained at %s\n' \
        "${run_root}" >&2
    exit 1
fi
manifest_hash_after="$(sha256sum -- "${profile_checksums}")"
manifest_hash_after="${manifest_hash_after%% *}"
manifest_hash_snapshot="$(sha256sum -- "${manifest_snapshot}")"
manifest_hash_snapshot="${manifest_hash_snapshot%% *}"
if [[ "${manifest_hash_before}" != "${manifest_hash_after}" || \
      "${manifest_hash_before}" != "${manifest_hash_snapshot}" ]]; then
    keep_run_root=1
    printf 'Checksum manifest changed during launch; run retained at %s\n' \
        "${run_root}" >&2
    exit 1
fi

manifest_expected_sha() {
    local source="$1"
    local line hash separator listed_path
    local found=""
    local count=0

    while IFS= read -r line || [[ -n "${line}" ]]; do
        if ((${#line} < 66)); then
            continue
        fi
        hash="${line:0:64}"
        separator="${line:64:2}"
        listed_path="${line:66}"
        if [[ "${separator}" == '  ' && "${listed_path}" == "${source}" ]]; then
            if [[ ! "${hash}" =~ ^[[:xdigit:]]{64}$ ]]; then
                printf 'Invalid checksum entry for launch artifact: %s\n' \
                    "${source}" >&2
                return 1
            fi
            found="${hash,,}"
            count=$((count + 1))
        fi
    done <"${manifest_snapshot}"

    if ((count != 1)); then
        printf 'Expected one checksum entry for %s, found %d.\n' \
            "${source}" "${count}" >&2
        return 1
    fi
    printf '%s\n' "${found}"
}

snapshot_launch_artifact() {
    local source="$1"
    local destination="$2"
    local label="$3"
    local expected actual copied

    if ! expected="$(manifest_expected_sha "${source}")"; then
        keep_run_root=1
        return 1
    fi
    actual="$(sha256sum -- "${source}")"
    actual="${actual%% *}"
    if [[ "${actual,,}" != "${expected}" ]]; then
        keep_run_root=1
        printf '%s changed before it could be snapshotted; run retained at %s\n' \
            "${label}" "${run_root}" >&2
        return 1
    fi
    if ! cp --preserve=mode,timestamps -- "${source}" "${destination}"; then
        keep_run_root=1
        printf 'Could not snapshot %s; run retained at %s\n' \
            "${label}" "${run_root}" >&2
        return 1
    fi
    copied="$(sha256sum -- "${destination}")"
    copied="${copied%% *}"
    if [[ "${copied,,}" != "${expected}" ]]; then
        keep_run_root=1
        printf '%s snapshot checksum mismatch; run retained at %s\n' \
            "${label}" "${run_root}" >&2
        return 1
    fi
}

snapshot_fixture_directory() {
    local source_directory="$1"
    local destination_directory="$2"
    local label="$3"
    local entry resolved_entry
    local count=0

    if [[ ! -d "${source_directory}" ]]; then
        keep_run_root=1
        printf 'Fixture directory is missing: %s\n' \
            "${source_directory}" >&2
        return 1
    fi
    mkdir -p -- "${destination_directory}"
    while IFS= read -r -d '' entry; do
        if [[ ! -f "${entry}" ]]; then
            keep_run_root=1
            printf '%s contains a non-file entry: %s\n' \
                "${label}" "${entry}" >&2
            return 1
        fi
        if ! resolved_entry="$(readlink -f -- "${entry}")"; then
            keep_run_root=1
            printf 'Could not resolve %s fixture entry: %s\n' \
                "${label}" "${entry}" >&2
            return 1
        fi
        snapshot_launch_artifact \
            "${resolved_entry}" \
            "${destination_directory}/$(basename -- "${entry}")" \
            "${label} fixture"
        count=$((count + 1))
    done < <(find "${source_directory}" -mindepth 1 -maxdepth 1 \
        -print0 | sort -z)
    if ((count == 0)); then
        keep_run_root=1
        printf '%s fixture directory is empty: %s\n' \
            "${label}" "${source_directory}" >&2
        return 1
    fi
}

snapshot_mutable_file() {
    local source="$1"
    local destination="$2"
    local label="$3"
    local before after copied

    if [[ -L "${source}" || ! -f "${source}" ]]; then
        keep_run_root=1
        printf '%s must be a regular non-symlink file: %s\n' \
            "${label}" "${source}" >&2
        return 1
    fi
    before="$(sha256sum -- "${source}")"
    before="${before%% *}"
    if ! cp --preserve=mode,timestamps -- "${source}" "${destination}"; then
        keep_run_root=1
        printf 'Could not snapshot %s; run retained at %s\n' \
            "${label}" "${run_root}" >&2
        return 1
    fi
    after="$(sha256sum -- "${source}")"
    after="${after%% *}"
    copied="$(sha256sum -- "${destination}")"
    copied="${copied%% *}"
    if [[ "${before}" != "${after}" || "${before}" != "${copied}" ]]; then
        keep_run_root=1
        printf '%s changed while being snapshotted; run retained at %s\n' \
            "${label}" "${run_root}" >&2
        return 1
    fi
}

launch_artifacts="${run_root}/launch-artifacts"
mkdir -- "${launch_artifacts}"
profile_fab_bin="${fab_bin}"
profile_vdp_module="${vdp_module}"
profile_mos_bin="${mos_bin}"
profile_mos_map="${mos_map}"
snapshot_launch_artifact \
    "${profile_fab_bin}" "${launch_artifacts}/fab-agon-emulator" \
    'Fab executable'
snapshot_launch_artifact \
    "${profile_vdp_module}" "${launch_artifacts}/vdp_combined.so" \
    'combined VDP module'
snapshot_launch_artifact \
    "${profile_mos_bin}" "${launch_artifacts}/mos_console8.bin" \
    'MOS image'
snapshot_launch_artifact \
    "${profile_mos_map}" "${launch_artifacts}/mos_console8.map" \
    'MOS symbol map'
fab_bin="${launch_artifacts}/fab-agon-emulator"
vdp_module="${launch_artifacts}/vdp_combined.so"
mos_bin="${launch_artifacts}/mos_console8.bin"

sdcard="${run_root}/sdcard"
mkdir -p -- "${sdcard}"
snapshot_fixture_directory \
    "${profile_sd}/mystuff/pingoasm/apps/earth-party-flat/tgt" \
    "${sdcard}/mystuff/pingoasm/apps/earth-party-flat/tgt" \
    'Pingo Earth Party Flat'
snapshot_fixture_directory \
    "${profile_sd}/pingo/cube-rgba2222/tgt" \
    "${sdcard}/pingo/cube-rgba2222/tgt" \
    'Pingo Cube smoke'
snapshot_fixture_directory \
    "${profile_sd}/wolf3d" "${sdcard}/wolf3d" 'Wolf3DOrig visual'
snapshot_fixture_directory \
    "${profile_sd}/combined-smoke" "${sdcard}/combined-smoke" \
    'Wolf combined smoke'

# Non-fixture convenience trees remain read-only links into the isolated
# combined profile. The exact launch fixtures above never run through them.
while IFS= read -r -d '' entry; do
    name="$(basename -- "${entry}")"
    case "${name}" in
        autoexec.txt|combined-smoke|mystuff|pingo|wolf3d)
            continue
            ;;
    esac
    ln -s -- "${entry}" "${sdcard}/${name}"
done < <(find "${profile_sd}" -mindepth 1 -maxdepth 1 -print0)

retain_smoke_log() {
    local source="$1"
    local destination="$2"
    local destination_parent destination_name temporary
    local source_resolved

    source_resolved="$(readlink -f -- "${source}")"
    if [[ "${source_resolved}" == "${destination}" || \
          ( -e "${destination}" && "${source}" -ef "${destination}" ) ]]; then
        keep_run_root=1
        printf 'Refusing to copy smoke log onto itself: %s\n' \
            "${destination}" >&2
        return 1
    fi
    if [[ -L "${destination}" || -e "${destination}" ]]; then
        keep_run_root=1
        printf 'Smoke-log destination appeared during the run: %s\n' \
            "${destination}" >&2
        return 1
    fi

    destination_parent="$(dirname -- "${destination}")"
    destination_name="$(basename -- "${destination}")"
    if ! temporary="$(mktemp --tmpdir="${destination_parent}" \
        ".${destination_name}.new.XXXXXX")"; then
        keep_run_root=1
        printf 'Could not create a temporary durable-log file; run retained at %s\n' \
            "${run_root}" >&2
        return 1
    fi
    if ! cp --preserve=mode,timestamps -- "${source}" "${temporary}"; then
        rm -f -- "${temporary}"
        keep_run_root=1
        printf 'Could not copy smoke log to %s; original retained at %s\n' \
            "${destination}" "${source}" >&2
        return 1
    fi
    if ! ln -- "${temporary}" "${destination}"; then
        rm -f -- "${temporary}"
        keep_run_root=1
        printf 'Could not publish smoke log at %s; original retained at %s\n' \
            "${destination}" "${source}" >&2
        return 1
    fi
    rm -f -- "${temporary}"
    printf 'Retained combined-emulator smoke log: %s\n' "${destination}"
}

if ((headless)); then
    printf '%s\r\n' \
        'SET KEYBOARD 1' \
        'cd /pingo/cube-rgba2222/tgt' \
        'load benchmark.bin' \
        'run' \
        'cd /combined-smoke' \
        'load wolf-dispatch-exit.bin' \
        'run' \
        >"${sdcard}/autoexec.txt"
else
    snapshot_mutable_file \
        "${profile_sd}/autoexec.txt" "${sdcard}/autoexec.txt" \
        'interactive autoexec'
fi

if [[ -f "${HOME}/.local/lib/libSDL3.so.0" ]]; then
    export LD_LIBRARY_PATH="${HOME}/.local/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
fi

cd -- "${run_root}"
if ((headless)); then
    log="${run_root}/headless-smoke.log"
    timeout_seconds="${COMBINED_EMULATOR_SMOKE_TIMEOUT:-60}"
    set +e
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout --signal=TERM "${timeout_seconds}s" \
        "${fab_bin}" \
            --renderer sw \
            --firmware console8 \
            --mos "${mos_bin}" \
            --vdp "${vdp_module}" \
            --sdcard "${sdcard}" \
            --verbose -z -u "$@" >"${log}" 2>&1
    result=$?
    set -e
    if [[ -n "${smoke_log_destination}" ]]; then
        if ! retain_smoke_log "${log}" "${smoke_log_destination}"; then
            exit 1
        fi
    fi
    if ((result != 0)); then
        keep_run_root=1
        printf 'Combined headless smoke exited with status %d.\n' "${result}" >&2
        exit "${result}"
    fi
    if ! grep -Fq 'initialize: pingo creating control structure' "${log}"; then
        keep_run_root=1
        printf '%s\n' 'Combined headless smoke saw no Pingo initialization record.' >&2
        exit 1
    fi
    pingo_render_count="$(grep -Fc 'PINGO_RENDER seq=' "${log}" || true)"
    if [[ "${pingo_render_count}" != 36 ]]; then
        keep_run_root=1
        printf 'Combined headless smoke expected 36 Pingo renders, found %s.\n' \
            "${pingo_render_count}" >&2
        exit 1
    fi
    if ! grep -Fq 'Wolf3D: Hello from Castle Wolfenstein 3D!' "${log}"; then
        keep_run_root=1
        printf '%s\n' 'Combined headless smoke saw no Wolf dispatch record.' >&2
        exit 1
    fi
    printf 'Combined headless smoke passed: Pingo Cube -> Wolf dispatch -> clean exit.\n'
    exit 0
fi

SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}" \
    "${fab_bin}" \
        --renderer sw \
        --firmware console8 \
        --mos "${mos_bin}" \
        --vdp "${vdp_module}" \
        --sdcard "${sdcard}" \
        --verbose -z "$@"
