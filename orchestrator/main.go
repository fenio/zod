// Zod match orchestrator - proof of concept.
//
// A tiny standalone service that spawns `zod -d` dedicated-server processes on
// demand and hands clients a host:port to connect to. Process-per-match: one
// `zod -d` per game session. All state is in memory and lost on restart - no
// database, no accounts, no auth. This is the spawn->connect loop and nothing
// more; everything else (a lobby UI, SQLite registry, identities) layers on top
// later.
//
// Build:  go build -o orchestrator .
// Run:    ZOD_DIR=/path/to/zod ZOD_BIN=/path/to/zod/build/zod ./orchestrator
//
// HTTP/JSON API:
//
//	POST   /matches    {"name":"...","map":"p02_bb_orig01.map","bots":["blue","green"]}
//	                   -> 201 {"id","name","map","bots","host","port"}
//	GET    /matches    -> 200 [ {match}, ... ]      (dead processes are reaped first)
//	GET    /matches/ID -> 200 {match}               (the join info)
//	DELETE /matches/ID -> 200 {"ok":true}           (kills the dedicated server)
//
// A client joins a returned match with:  ZOD_PORT=<port> zod -c <host> -t <team> -n <name>
package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

// validTeams are the only strings accepted as bot teams, so a bogus value can
// never reach the spawned command line.
var validTeams = map[string]bool{"red": true, "blue": true, "green": true, "yellow": true}

// Match is one running dedicated-server process.
type Match struct {
	ID      string    `json:"id"`
	Name    string    `json:"name"`
	Map     string    `json:"map"`
	Bots    []string  `json:"bots"`
	Host    string    `json:"host"`
	Port    int       `json:"port"`
	PID     int       `json:"pid"`
	Players int       `json:"players"` // humans in the match; -1 until the server writes its status file
	Created time.Time `json:"created"`

	cmd        *exec.Cmd `json:"-"`
	statusSock string    `json:"-"` // local AF_UNIX socket the server answers player-count queries on
	logFile    string    `json:"-"`
}

// Orchestrator owns all live matches and the pool of ports they can use.
type Orchestrator struct {
	zodBin    string // path to the zod binary
	zodDir    string // working dir (must contain maps/)
	advertise string // host clients should connect to (this machine's public addr)
	emptyExit int    // seconds a match may sit empty before the server self-exits (0 = never)

	mu        sync.Mutex
	matches   map[string]*Match
	freePorts []int
}

func newOrchestrator(zodBin, zodDir, advertise string, emptyExit, portMin, portMax int) *Orchestrator {
	ports := make([]int, 0, portMax-portMin+1)
	for p := portMin; p <= portMax; p++ {
		ports = append(ports, p)
	}
	return &Orchestrator{
		zodBin:    zodBin,
		zodDir:    zodDir,
		advertise: advertise,
		emptyExit: emptyExit,
		matches:   make(map[string]*Match),
		freePorts: ports,
	}
}

// createMatch spawns a `zod -d` for the given map + bots and returns the match.
// Caller passes already-validated inputs.
func (o *Orchestrator) createMatch(name, mapName string, bots []string) (*Match, error) {
	o.mu.Lock()
	if len(o.freePorts) == 0 {
		o.mu.Unlock()
		return nil, fmt.Errorf("no free ports: server is at match capacity")
	}
	port := o.freePorts[0]
	o.freePorts = o.freePorts[1:]
	o.mu.Unlock()

	// Build the argv directly (no shell) so user input can't be interpreted.
	args := []string{"-d", "-m", filepath.Join("maps", mapName)}
	for _, b := range bots {
		args = append(args, "-b", b)
	}

	// The server writes its live player count to this LOCAL file (never over the
	// network), which we read back for the listing. ZOD_EMPTY_EXIT_SECS lets the
	// server self-exit when it's sat empty, so our wait() goroutine reaps it.
	sockPath := filepath.Join(os.TempDir(), fmt.Sprintf("zod-status-%d.sock", port))
	logPath := filepath.Join(os.TempDir(), fmt.Sprintf("zod-match-%d.log", port))

	cmd := exec.Command(o.zodBin, args...)
	cmd.Dir = o.zodDir
	cmd.Env = append(os.Environ(),
		fmt.Sprintf("ZOD_PORT=%d", port),
		"ZOD_AUTOSTART=1", // don't sit paused on the start screen with no human yet
		"ZOD_STATUS_SOCK="+sockPath,
		fmt.Sprintf("ZOD_EMPTY_EXIT_SECS=%d", o.emptyExit),
	)
	// Per-match log so a wedged server can be inspected.
	if lf, err := os.Create(logPath); err == nil {
		cmd.Stdout = lf
		cmd.Stderr = lf
	}

	if err := cmd.Start(); err != nil {
		o.releasePort(port)
		return nil, fmt.Errorf("failed to start zod -d: %w", err)
	}

	m := &Match{
		ID:         newID(),
		Name:       name,
		Map:        mapName,
		Bots:       bots,
		Host:       o.advertise,
		Port:       port,
		PID:        cmd.Process.Pid,
		Players:    -1, // unknown until the server writes its first status snapshot
		Created:    time.Now().UTC(),
		cmd:        cmd,
		statusSock: sockPath,
		logFile:    logPath,
	}

	o.mu.Lock()
	o.matches[m.ID] = m
	o.mu.Unlock()

	// One goroutine per match waits for the process to exit (a crash, or a
	// DELETE that killed it) and reaps it - removing the entry and returning
	// the port to the pool. No polling, no zombies.
	go func() {
		cmd.Wait()
		o.mu.Lock()
		delete(o.matches, m.ID)
		o.freePortLocked(port)
		o.mu.Unlock()
		os.Remove(sockPath)
		os.Remove(logPath)
		log.Printf("match %s reaped (port %d freed)", m.ID, port)
	}()

	log.Printf("match %s started: map=%s bots=%v port=%d pid=%d", m.ID, mapName, bots, port, m.PID)
	return m, nil
}

func (o *Orchestrator) releasePort(port int) {
	o.mu.Lock()
	o.freePortLocked(port)
	o.mu.Unlock()
}

func (o *Orchestrator) freePortLocked(port int) {
	o.freePorts = append(o.freePorts, port)
}

func (o *Orchestrator) list() []*Match {
	o.mu.Lock()
	defer o.mu.Unlock()
	out := make([]*Match, 0, len(o.matches))
	for _, m := range o.matches {
		m.Players = readPlayers(m.statusSock)
		out = append(out, m)
	}
	return out
}

func (o *Orchestrator) get(id string) *Match {
	o.mu.Lock()
	defer o.mu.Unlock()
	m := o.matches[id]
	if m != nil {
		m.Players = readPlayers(m.statusSock)
	}
	return m
}

// readPlayers asks the server for its current human-player count by connecting to
// its local AF_UNIX admin socket and reading the JSON it writes back. On demand -
// nothing is polled or stored. Returns -1 if the socket isn't up yet or errors.
func readPlayers(sock string) int {
	if sock == "" {
		return -1
	}
	conn, err := net.DialTimeout("unix", sock, 250*time.Millisecond)
	if err != nil {
		return -1
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(250 * time.Millisecond))
	data, err := io.ReadAll(conn)
	if err != nil {
		return -1
	}
	var s struct {
		Players int `json:"players"`
	}
	if json.Unmarshal(data, &s) != nil {
		return -1
	}
	return s.Players
}

// kill terminates a match's process. The per-match reaper goroutine then removes
// it from the map and frees the port.
func (o *Orchestrator) kill(id string) bool {
	o.mu.Lock()
	m := o.matches[id]
	o.mu.Unlock()
	if m == nil {
		return false
	}
	_ = m.cmd.Process.Kill()
	return true
}

// --- HTTP layer ---

type createReq struct {
	Name string   `json:"name"`
	Map  string   `json:"map"`
	Bots []string `json:"bots"`
}

func (o *Orchestrator) handleCreate(w http.ResponseWriter, r *http.Request) {
	var req createReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		httpErr(w, http.StatusBadRequest, "invalid JSON body")
		return
	}

	// Validate the map: a bare filename that exists under maps/. Reject anything
	// with a path separator so a request can't escape the maps directory.
	if req.Map == "" || strings.ContainsAny(req.Map, `/\`) || strings.Contains(req.Map, "..") {
		httpErr(w, http.StatusBadRequest, "map must be a bare filename")
		return
	}
	if _, err := os.Stat(filepath.Join(o.zodDir, "maps", req.Map)); err != nil {
		httpErr(w, http.StatusBadRequest, "no such map: "+req.Map)
		return
	}

	// Validate bots against the fixed team set.
	for _, b := range req.Bots {
		if !validTeams[b] {
			httpErr(w, http.StatusBadRequest, "invalid bot team: "+b)
			return
		}
	}

	name := req.Name
	if name == "" {
		name = "match"
	}

	m, err := o.createMatch(name, req.Map, req.Bots)
	if err != nil {
		httpErr(w, http.StatusServiceUnavailable, err.Error())
		return
	}
	writeJSON(w, http.StatusCreated, m)
}

func (o *Orchestrator) handleList(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, http.StatusOK, o.list())
}

func (o *Orchestrator) handleGet(w http.ResponseWriter, r *http.Request) {
	m := o.get(r.PathValue("id"))
	if m == nil {
		httpErr(w, http.StatusNotFound, "no such match")
		return
	}
	writeJSON(w, http.StatusOK, m)
}

func (o *Orchestrator) handleDelete(w http.ResponseWriter, r *http.Request) {
	if !o.kill(r.PathValue("id")) {
		httpErr(w, http.StatusNotFound, "no such match")
		return
	}
	writeJSON(w, http.StatusOK, map[string]bool{"ok": true})
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(v)
}

func httpErr(w http.ResponseWriter, code int, msg string) {
	writeJSON(w, code, map[string]string{"error": msg})
}

func newID() string {
	var b [4]byte
	rand.Read(b[:])
	return hex.EncodeToString(b[:])
}

func env(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func main() {
	zodDir := env("ZOD_DIR", ".")
	zodBin := env("ZOD_BIN", filepath.Join(zodDir, "build", "zod"))
	addr := env("ORCH_ADDR", ":8080")
	advertise := env("ADVERTISE_HOST", "127.0.0.1")
	emptyExit, _ := strconv.Atoi(env("EMPTY_EXIT_SECS", "300")) // empty match self-exits after 5 min

	o := newOrchestrator(zodBin, zodDir, advertise, emptyExit, 2300, 2399)

	mux := http.NewServeMux()
	mux.HandleFunc("POST /matches", o.handleCreate)
	mux.HandleFunc("GET /matches", o.handleList)
	mux.HandleFunc("GET /matches/{id}", o.handleGet)
	mux.HandleFunc("DELETE /matches/{id}", o.handleDelete)
	mux.HandleFunc("GET /", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintln(w, "zod match orchestrator (PoC) - POST/GET/DELETE /matches")
	})

	log.Printf("orchestrator listening on %s | zod=%s dir=%s advertise=%s ports=2300-2399",
		addr, zodBin, zodDir, advertise)
	log.Fatal(http.ListenAndServe(addr, mux))
}
