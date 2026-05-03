# SDK Reset and Demo Replacement Design

## Goal

Recover repository from dirty local SDK state by preserving current main-tree chassis controller source for future rewrite, replacing `smartsens_sdk` with upstream official contents, replacing `ssne_ai_demo` with `demo-rps`, removing abandoned ROS-related paths, restoring a working EVB build script, and making Docker use the host-mounted `data/A1_SDK_SC132GS` tree as the single source of truth.

## Scope

In scope:
- Back up current main-tree `chassis_controller` related source into `@/memories/repo/chassis_controller_backup.md`
- Remove existing Claude worktrees for this repository
- Replace `data/A1_SDK_SC132GS/smartsens_sdk/` with upstream official SDK contents
- Replace `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/` with same-named directory from `demo-rps`
- Update Docker configuration so host `data/A1_SDK_SC132GS` is bind-mounted into container at `/app/data/A1_SDK_SC132GS`
- Rewrite `scripts/build_complete_evb.sh` as non-ROS EVB build entrypoint with working `--app-only`
- Remove or rewrite ROS-related build entrypoints and stale references that conflict with current direction

Out of scope:
- Preserving any old worktree copy as source of truth
- Merging local dirty SDK changes into upstream SDK
- Retaining ROS feature support
- Editing unrelated vendor trees outside requested replacement boundary

## Design Decisions

### 1. Backup boundary

Backup captures only current main-tree real source.

Include:
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/` files that define `chassis_controller`
- Files in same current main-tree demo that directly reference or integrate with that code

Exclude:
- `.claude/worktrees/**`
- `output/**`
- Container-only copies
- Historical or generated artifacts

Reasoning:
- User wants future rewrite material from current main tree only
- Old worktrees and generated output would reintroduce dirty or stale states

Backup document must contain:
- Original file paths
- Full source blocks for each backed up file section
- Direct reference points where backed-up code is used
- Restore notes describing intended destination for future rewrite

### 2. Source-of-truth model

Repository main tree becomes sole editable source of truth.

- Local host path `data/A1_SDK_SC132GS` remains canonical
- Docker must consume same directory through bind mount
- No container-private SDK fork or patching flow remains
- Claude worktrees are removed and not used by default going forward

Reasoning:
- Avoid local/container drift
- Avoid recovery work accidentally pulling from stale worktree copies
- Match user preference to stop using worktrees for this project

### 3. SDK replacement model

`data/A1_SDK_SC132GS/smartsens_sdk/` is replaced wholesale from official upstream repository.

Approach:
- Delete existing local `smartsens_sdk/`
- Fresh shallow clone upstream official SDK into same path
- Do not merge dirty local files back in

Reasoning:
- User explicitly wants complete overwrite by remote contents
- Partial merge would preserve unknown contamination

Boundary:
- Preserve parent `data/A1_SDK_SC132GS/` directory so other colocated content and Docker mount root remain stable
- Only SDK subtree is reset from upstream

### 4. Demo replacement model

`ssne_ai_demo` inside official SDK tree is then replaced wholesale by same-named directory from `demo-rps`.

Approach:
- Fetch `demo-rps`
- Copy only `smart_software/src/app_demo/face_detection/ssne_ai_demo/` equivalent payload into local SDK tree target
- Do not import unrelated repository content unless required to make same-named demo directory complete

Reasoning:
- Official SDK should define baseline platform tree
- `demo-rps` should define board-side demo behavior
- This keeps replacement boundaries explicit: official SDK for platform, `demo-rps` for demo

### 5. ROS abandonment

ROS is treated as abandoned scope, not optional scope.

Implications:
- New `scripts/build_complete_evb.sh` contains no ROS branches
- Remove obsolete `--skip-ros` path
- Remove or rewrite stale scripts and docs that claim full EVB build depends on ROS
- No attempt to preserve compatibility with `src/ros2_ws/` workflows in this task

Reasoning:
- User explicitly abandoned ROS functionality
- Keeping dormant ROS branches would add false complexity and broken paths

### 6. Build script model

`scripts/build_complete_evb.sh` is rewritten as single non-ROS EVB build entrypoint.

Required modes:
- Default full EVB build
- `--app-only`
- `--clean`

Default full EVB build behavior:
1. Validate required local SDK path exists under repository
2. Validate required container and bind mount assumptions
3. Rebuild `ssne_ai_demo`
4. Re-run SDK packaging/image build
5. Collect final firmware into `output/evb/<timestamp>/zImage.smartsens-m1-evb`
6. Update `output/evb/latest` link or equivalent current-project convention if still used

`--app-only` behavior:
1. Validate baseline SDK build cache/prerequisites exist
2. Rebuild only `ssne_ai_demo`
3. Repackage final image so newest app lands in final `zImage`
4. Skip any full base-layer rebuild logic

`--clean` behavior:
- Clean script-owned temporary/build outputs needed for this flow
- Must not delete user source trees or host bind-mounted SDK sources

Script constraints:
- Operate against repository-local `data/A1_SDK_SC132GS/smartsens_sdk/`
- Be compatible with `docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"`
- Use LF line endings
- Emit explicit failures for missing SDK, missing container, broken mount, or missing downstream build targets

### 7. Docker mount model

Docker runtime/build configuration must preserve full bind mount:
- Host: `data/A1_SDK_SC132GS`
- Container: `/app/data/A1_SDK_SC132GS`

Expected behavior:
- Local edits under host SDK tree immediately appear inside container
- No follow-up manual copy into container required
- Recreated containers still see same host SDK tree

Possible touchpoints:
- `docker/docker-compose.yml`
- helper scripts that launch or document the container
- related docs that describe the build environment

Exact file changes depend on current config state discovered during implementation.

## Execution Flow

1. Inspect current main-tree `ssne_ai_demo` for `chassis_controller` definitions and direct references
2. Write `@/memories/repo/chassis_controller_backup.md`
3. Enumerate and remove Claude worktrees associated with this repository
4. Replace local `smartsens_sdk/` from official upstream repository
5. Replace local `ssne_ai_demo/` from `demo-rps`
6. Update Docker configuration to enforce host bind-mounted `data/A1_SDK_SC132GS`
7. Rewrite `scripts/build_complete_evb.sh` for non-ROS EVB packaging flow
8. Remove or rewrite stale ROS-facing scripts/docs that conflict with new flow
9. Verify backup, replacement boundaries, Docker mount behavior, and build script entrypoints

## Verification Strategy

### Backup verification
Confirm `@/memories/repo/chassis_controller_backup.md` includes:
- Backed-up file paths
- Full relevant code blocks
- Direct references/integration points
- Rewrite destination notes

### Replacement verification
Confirm:
- `data/A1_SDK_SC132GS/smartsens_sdk/` matches fresh upstream layout
- `ssne_ai_demo/` contents come from `demo-rps`
- ROS-specific branches or entrypoints targeted by this task are gone or rewritten
- Old Claude worktrees for this repo are removed

### Docker verification
Confirm:
- Container sees `/app/data/A1_SDK_SC132GS`
- Host-side touch/change under mounted tree is visible inside container
- No separate container-only SDK replacement flow remains

### Build verification
Confirm:
- `bash scripts/build_complete_evb.sh --app-only` starts and reaches expected build path
- Default mode starts and reaches expected full EVB path
- Output path messaging points to `output/evb/<timestamp>/zImage.smartsens-m1-evb`
- Failures, if any, identify exact missing prerequisite

## Risks and mitigations

### Risk: Wrong source chosen for backup
Mitigation: back up only current main-tree files under explicit target path, never worktree/output copies.

### Risk: Container still uses stale internal SDK copy
Mitigation: verify bind mount configuration and visible host/container synchronization after replacement.

### Risk: `demo-rps` layout differs from current SDK target
Mitigation: inspect fetched repository structure before copy and adjust copy source path while preserving destination boundary.

### Risk: Rewritten build script targets removed or renamed upstream build hooks
Mitigation: inspect actual official SDK build entrypoints after replacement before finalizing script commands.

### Risk: CRLF breaks board scripts
Mitigation: ensure rewritten shell scripts are saved with LF and verify line endings before completion.

## Success criteria

Task succeeds when all are true:
- `chassis_controller` backup exists under `@/memories/repo/chassis_controller_backup.md`
- Claude worktrees for this repo are removed
- Local `smartsens_sdk` is freshly replaced from official upstream
- Local `ssne_ai_demo` is replaced from `demo-rps`
- Docker uses host bind-mounted `data/A1_SDK_SC132GS` as single SDK source
- `scripts/build_complete_evb.sh` is restored as non-ROS EVB build entrypoint with working `--app-only`
- Stale ROS-dependent build flow references are removed or rewritten to match abandoned ROS direction
