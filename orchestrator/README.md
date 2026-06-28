# Zod match orchestrator (PoC)

A tiny standalone service that spawns `zod -d` dedicated-server processes on
demand and hands clients a `host:port` to connect to. **Process-per-match**: one
`zod -d` per game session.

This is a proof of concept — the spawn → connect loop and nothing else:

- **No database** — all state is in memory, lost on restart.
- **No accounts, no auth** — anyone who can reach the API can create/kill matches.
- **No lobby UI** — it's a JSON HTTP API; the in-game browser comes later.

Single static Go binary, stdlib only (no external modules).

## Build

```sh
cd orchestrator
go build -o orchestrator .
```

## Run

```sh
ZOD_DIR=/path/to/zod \
ZOD_BIN=/path/to/zod/build/zod \
ADVERTISE_HOST=your.public.host \
ORCH_ADDR=:8080 \
./orchestrator
```

| Env var | Default | Meaning |
|---|---|---|
| `ZOD_DIR` | `.` | Working dir for spawned servers; must contain `maps/`. |
| `ZOD_BIN` | `$ZOD_DIR/build/zod` | Path to the `zod` binary. |
| `ADVERTISE_HOST` | `127.0.0.1` | Host returned to clients (this machine's public address). |
| `ORCH_ADDR` | `:8080` | Address the HTTP API binds to. |
| `EMPTY_EXIT_SECS` | `300` | A match whose server sits empty this long self-exits and is reaped. |
| `MM_CAPACITY` | `2` | Human players an open (matchmaking) match accepts before it's full (1v1 PoC). |
| `MM_MAP` | first map | Preferred map for matchmaking matches; falls back to the first `.map`. |
| `MATCH_PORT_MIN` | `2300` | Low end of the match port pool. **Must** be within the ports you publish + firewall. |
| `MATCH_PORT_MAX` | `2399` | High end of the match port pool (one port = one concurrent match). |

Match ports are allocated from **`MATCH_PORT_MIN`–`MATCH_PORT_MAX`** (one per
concurrent match). **Critical:** this range must equal the ports you actually
publish to the host *and* open in the firewall — the orchestrator hands clients
`ADVERTISE_HOST:<port>`, and a port it allocates but you didn't expose is a dead
match. The Docker image presets these to **2300-2305** (a 6-match pool); publish
that same range. e.g. for more: `-e MATCH_PORT_MIN=2300 -e MATCH_PORT_MAX=2309 -p 2300-2309:2300-2309`.

## Docker

The `Dockerfile` at the repo root builds a self-contained server image (~67 MB):
the orchestrator + the headless `zod -d` engine + the game data it reads. `zod -d`
never opens a window or audio device, so no X server / xvfb is needed; the final
image is `FROM scratch` carrying only the binaries' shared-lib closure.

**Pull the prebuilt image** — multi-arch (`linux/amd64` + `linux/arm64`), pushed
to GHCR by CI on every `master` push / tag; Docker pulls the right arch for your
host automatically:

```sh
docker run --rm -p 8080:8080 -p 2300-2305:2300-2305 \
  -e ADVERTISE_HOST=your.public.ip \
  ghcr.io/fenio/zod-server:latest
```
(The image defaults to a 6-match pool, `MATCH_PORT_MIN/MAX=2300/2305`. For more
matches, bump the published range *and* `MATCH_PORT_MIN/MAX` together — the
published range and the pool must match exactly.)

**Or build it yourself:**

```sh
docker build -t zod-server .                 # from repo root
docker run --rm -p 8080:8080 -p 2300-2305:2300-2305 \
  -e ADVERTISE_HOST=your.public.ip zod-server
# or: ADVERTISE_HOST=your.public.ip docker compose up --build
```

`ADVERTISE_HOST` must be an address joining clients can actually reach;
`127.0.0.1` only works for same-host testing. A plain `docker build` produces an
image for the host's arch; for the other arch (or both) use buildx, e.g.
`docker buildx build --platform linux/amd64 -t zod-server .` or
`--platform linux/amd64,linux/arm64`.

The same `ZOD_DIR`/`ZOD_BIN`/`ADVERTISE_HOST`/`ORCH_ADDR`/`EMPTY_EXIT_SECS`
env vars apply; the image presets `ZOD_DIR=/app`, `ZOD_BIN=/app/build/zod`.

**Debugging:** the scratch image has no shell, so `docker exec` won't give you a
prompt. Use `docker logs <container>` for the orchestrator output, and pull a
match server's log out with `docker cp <container>:/tmp/zod-match-<port>.log .`.

## API

```
POST   /matchmake    -> 200 {match}        (matchmaking: the open match to drop a
                                            "play with someone" joiner into; seeds a
                                            fresh paused, bot-less one if none has room)
POST   /matches      {"name":"...","map":"p02_bb_orig01.map","bots":["blue","green"]}
                     -> 201 {"id","name","map","bots","host","port","pid","created"}
GET    /matches      -> 200 [ {match}, ... ]
GET    /matches/{id} -> 200 {match}        (the join info)
DELETE /matches/{id} -> 200 {"ok":true}    (kills the dedicated server)
GET    /maps         -> 200 ["a.map", ...] (.map files the create form can pick)
```

`map` must be a bare filename present under `maps/`. `bots` is any subset of
`red green blue yellow`. Inputs are validated and passed as argv (no shell), so
a request can't escape the maps dir or inject flags. `GET /maps` exists because
the in-game create form needs the real filenames — the client only knows
campaign *display* names.

### Example

```sh
# create
curl -s -X POST localhost:8080/matches \
  -d '{"name":"friday night","map":"p02_bb_orig01.map","bots":["blue","green"]}'
# -> {"id":"3c0d4c57",...,"host":"your.host","port":2300}

# list
curl -s localhost:8080/matches

# join (on a player's machine)
ZOD_PORT=2300 zod -c your.host -t red -n YourName

# kill
curl -s -X DELETE localhost:8080/matches/3c0d4c57
```

## How a match's lifecycle works

- On `POST`, a port is taken from the pool and `zod -d -m maps/<map> -b <bots…>`
  is started with `ZOD_PORT=<port>`, `ZOD_AUTOSTART=1` (so it doesn't sit paused
  waiting for a human), `ZOD_STATUS_SOCK=<local path>`, and `ZOD_EMPTY_EXIT_SECS`.
  `-d` binds all interfaces, so remote players can connect at `ADVERTISE_HOST:port`.
- **Player counts** are pulled on demand over a local **AF_UNIX admin socket**: the
  server listens on `ZOD_STATUS_SOCK`, and when the orchestrator serves a listing
  it connects, reads back `{"players":N,...}`, and disconnects. A query, not a
  periodic push — nothing is written on a timer or kept in a data file. It counts
  humans only (bots connect as clients but are excluded). The socket is local and
  **never reachable over the network**, so players connecting to a match can't see
  it. (On a Windows host the socket is skipped; the orchestrator runs on Linux.)
- **Auto-reap.** The server self-exits once it's sat empty for `EMPTY_EXIT_SECS`
  (a match nobody ever joins is cleaned up too). One goroutine per match waits on
  the process; when it exits — self-exit, crash, or a `DELETE` — the match is
  removed, its port returned to the pool, and its socket and log files deleted.
  No polling, no zombies.
- Each match logs to `$TMPDIR/zod-match-<port>.log` for inspection.

## Known PoC gaps (next steps)

- **No persistence.** Restarting the orchestrator forgets all matches (the
  `zod -d` processes keep running, orphaned). A SQLite registry fixes this.
- **No auth / rate limiting.** Open create = trivial DoS (spawn 100 processes).
  Fine behind a firewall / among friends; add a token before exposing it.
- **No readiness wait.** The port is returned immediately; a client that
  connects in the first ~100ms relies on the client's own connect-retry.
