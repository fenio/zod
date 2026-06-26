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

Match ports are allocated from **2300–2399** (cap = 100 concurrent matches).

## API

```
POST   /matches      {"name":"...","map":"p02_bb_orig01.map","bots":["blue","green"]}
                     -> 201 {"id","name","map","bots","host","port","pid","created"}
GET    /matches      -> 200 [ {match}, ... ]
GET    /matches/{id} -> 200 {match}        (the join info)
DELETE /matches/{id} -> 200 {"ok":true}    (kills the dedicated server)
```

`map` must be a bare filename present under `maps/`. `bots` is any subset of
`red green blue yellow`. Inputs are validated and passed as argv (no shell), so
a request can't escape the maps dir or inject flags.

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
