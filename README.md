# abls-agent-server

Template runtime for a future Abls-Habitat server agent.

## Current implementation status

- Runtime skeleton based on ABLS-AGENT-LIBS
- Facility fixed to `server`
- Prefix initialized from `agent_tech_id`
- Main lifecycle implemented with:
  - `Agent_init(...)`
  - `Agent_loop(...)`
  - `Agent_end(...)`
- `src/server.c` is intentionally minimal and ready to extend

## Build

```sh
./install_deps.sh
./build.sh
```

## Packaging RPM

```sh
./build_rpm.sh
```

Produces runtime RPM package in `build/`.

## Packaging DEB

```sh
./build_apt.sh --dist bookworm --no-sign
./build_apt.sh --dist trixie --no-sign
```

Produces runtime DEB package and copies normalized artifacts to:

- `build/deb/<suite>/<arch>/`

## Release bump + publication

```sh
./bump.sh 1.2.3
```

The release flow:

- tags `v1.2.3` from `trunk`
- merges `trunk` into `main`
- builds RPM + DEB packages
- copies RPM to `../ABLS-PKGS/public/rpms/<arch>/`
- copies DEB to `../ABLS-PKGS/deb-packages/<suite>/`

## Container build

```sh
podman build -t abls-agent-server:dev \
  --build-arg ABLS_LIBS_DEVEL_RPM_URL=<url> \
  --build-arg ABLS_AGENT_LIBS_DEVEL_RPM_URL=<url> \
  --build-arg ABLS_LIBS_RPM_URL=<url> \
  --build-arg ABLS_AGENT_LIBS_RPM_URL=<url> \
  -f Containerfile .
```
