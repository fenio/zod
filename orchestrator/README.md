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
  is started with `ZOD_PORT=<port>` and `ZOD_AUTOSTART=1` (so it doesn't sit
  paused waiting for a human). `-d` binds all interfaces, so remote players can
  connect at `ADVERTISE_HOST:port`.
- One goroutine per match waits on the process. When it exits — crash, or a
  `DELETE` that killed it — the match is removed and its port returned to the
  pool. No polling, no zombies.
- Each match logs to `$TMPDIR/zod-match-<port>.log` for inspection.

## Known PoC gaps (next steps)

- **No player counts / empty-match reaping.** `zod -d` doesn't exit when the
  last player leaves (it cycles maps), and exposes no player count, so idle
  matches linger until `DELETE`d. A real version needs a small admin/status
  signal from the game binary (player count, "empty for N minutes → exit").
- **No persistence.** Restarting the orchestrator forgets all matches (the
  `zod -d` processes keep running, orphaned). A SQLite registry fixes this.
- **No auth / rate limiting.** Open create = trivial DoS (spawn 100 processes).
  Fine behind a firewall / among friends; add a token before exposing it.
- **No readiness wait.** The port is returned immediately; a client that
  connects in the first ~100ms relies on the client's own connect-retry.
