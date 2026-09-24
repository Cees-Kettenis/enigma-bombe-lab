# Enigma Bombe Lab development notes

A native Linux desktop laboratory for Enigma I encryption and crib-based cryptanalysis. The interface uses GTK 4 and Cairo. The cipher, menu solver, persistent pthread pool and benchmark have no GTK dependency. Everything runs locally, with no downloaded assets or runtime network services.

The Enigma implements rotors I-V, reflectors B/C, Ringstellung, the middle rotor's double step, and up to ten plugboard pairs. The Bombe uses real menu constraints to derive plugboard mappings. It never receives the challenge generator's secret key or hidden plaintext.

For setup and everyday use, see the [main README](../README.md). Run the commands below from the project root.

## Install dependencies

Requires a C17 compiler, GTK 4.8 or newer, GLib, Cairo, POSIX threads, Meson, Ninja and pkg-config. GTK's development package supplies the GLib/Cairo dependencies and resource compiler. GCC and Clang are supported. No C++, Python, JavaScript, WebView or external images are used by the application. Meson itself is a build-time tool with its own distribution-packaged dependencies.

Ubuntu 24.04+, or Debian 12+:

```sh
sudo apt update
sudo apt install build-essential meson ninja-build pkg-config libgtk-4-dev
```

Fedora:

```sh
sudo dnf install gcc meson ninja-build pkgconf-pkg-config gtk4-devel
```

Arch Linux:

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf gtk4
```

A running Wayland or X11 desktop is required for the GUI. The core tests do not require a display. Older distributions with GTK below 4.8 need a newer GTK development package.

## Build and run

From the project directory:

```sh
meson setup build --buildtype=release
meson compile -C build
./build/enigma-bombe-lab
```

Meson uses Ninja by default. Release builds use `-O3`; all project C sources build with `-Wall -Wextra -Wpedantic -Wshadow`. Third-party GTK headers are system includes. CSS is compiled into the executable with GResource, so the application can run from any working directory.

No sudo is needed to build or run the application.

```sh
./build/enigma-bombe-lab --threads 16
./build/enigma-bombe-lab --benchmark
./build/enigma-bombe-lab examples/training.ini
./build/enigma-bombe-lab --help
```

`--benchmark` opens the GUI and starts its background benchmark. It does not replace the GUI with a terminal program. Worker count defaults to the online logical processor count, up to the application limit of 1,024. Creating more threads than the OS permits produces a recoverable error.

An optional build optimized for this CPU:

```sh
meson setup build-native --buildtype=release -Dc_args='-O3 -march=native'
meson compile -C build-native
./build-native/enigma-bombe-lab
```

Do not distribute a `-march=native` binary to machines with different CPU capabilities. The regular release build is the portable scalar implementation.

Clang build:

```sh
CC=clang meson setup build-clang --buildtype=release -Dwerror=true
meson compile -C build-clang
meson test -C build-clang --print-errorlogs
```

## First recovery

1. Open **MESSAGE / INTERCEPT**, then press **Example intercept**. Alternatively, open `examples/training.ini` at startup.
2. The generated key and plaintext are hidden. The example supplies a long pangram crib at offset zero. This deliberately strong crib makes a complete recovery easy to inspect.
3. In **BOMBE**, choose 1, 2, 4, 8, all logical CPUs, or a custom number of workers.
4. Press **START BOMBE**. The training search covers all 60 ordered selections of three rotors and all 17,576 starting windows.
5. Watch real state counts, contradictions, throughput and stops. The example's correct state happens to be near the start, but the solver still searches the entire space.
6. Double-click the recovered stop, or focus its row and press Enter. This loads its configuration into Enigma and its full plaintext into the message desk.

The example is intentionally easier than an authentic intercepted message. Its crib contains every letter. A short `WETTERBERICHT` crib can produce many stops and may leave much of the plugboard unknown.

The guided tutorial is available from the header or `--tutorial`. `Alt+1` through `Alt+6` select the six tabs in order.

## Encrypt and create challenges

In **ENIGMA**, select three distinct rotors from left to right, choose reflector B or C, and enter three letters for rings and start windows. Press **Apply / reset**. Letters A-Z correspond to historical ring numbers 01-26.

Click the keyboard entry and press A-Z. Each letter appends to the message, computes its ciphertext and animates the latest real keypress. The message desk also offers batch encryption and an **Encrypt as I type** option. Slow, normal, fast and instant animation change only the display. GTK's reduced-motion preference disables motion.

Encryption always begins at the configured start windows. To decrypt, put ciphertext into the plaintext input and encrypt with the same configuration. Reset returns the displayed windows to the configured start. Input retains ASCII A-Z, uppercases lowercase letters, and discards spaces, punctuation, digits and other characters. Offsets count normalized letters, not original text bytes.

In **PLUGBOARD**, click two sockets to connect a cable. Clicking a connected socket removes it. The textual editor provides a keyboard-accessible alternative, such as `AG BL CZ`. Duplicate letters, self-pairs and more than ten pairs are rejected. **Randomize 10 pairs** uses twenty distinct letters.

**Create random intercept** encrypts the current plaintext with a separately generated rotor order, start windows and ten plugboard pairs. Empty plaintext uses the example message. The checkbox on the Enigma page enables random rings. The selected reflector is retained. The generator stores the secret in a separate `Challenge` object and clears the editable machine's secret settings and plaintext. Training rings and reflector remain public. Reveal/hide affects only the GUI.

Randomization uses a local pseudorandom generator for simulation. This software is not a modern secure cipher or a secure key generator.

## Menus and the solver

Choose a crib and a zero-based offset in **CRIB / MENU**. Previous/next shifts one position. Automatic alignment selects the valid offset with the most independent cycles, breaking ties by the highest node degree. It does not know the true alignment and does not automatically search every alignment.

An alignment is impossible if a crib letter equals the ciphertext letter at the same position. Enigma cannot encrypt a letter to itself. The menu draws letters as nodes, relationships as edges and message positions as labels. Amber edges close cycles in a spanning forest; the highlighted seed has the highest degree.

For candidate rotor settings, let `S_i` be the rotor/reflector permutation after the machine has stepped for message letter `i`, and let `P` be its plugboard involution. The menu enforces:

```text
c_i = P(S_i(P(p_i)))
P(c_i) = S_i(P(p_i))
```

For each rotor state the engine:

1. Advances through the crib offset with the actual double-stepping mechanism and builds the 26-letter `S_i` tables.
2. Chooses the highest-degree unresolved menu letter and tries its possible plugboard partners.
3. Propagates known assignments across menu edges in both directions. Assigning `P(A)=G` simultaneously assigns `P(G)=A`.
4. Rejects unequal forced values, reused partners and more than ten transpositions immediately.
5. Recurses only when unresolved menu components need another assumption.
6. Records surviving deductions. Letters never constrained by the menu remain explicitly unresolved.
7. Uses identity for those unresolved letters, resets the candidate Enigma, decrypts the ciphertext, and checks the entire crib again before publishing the stop.

There is no enumeration of all complete plugboards. The recursion assigns only letters needed by menu constraints and their involution partners. No allocation or string search occurs in the cipher loop. Candidate scoring runs only after a stop, using a modest English letter-frequency score and several English/German sequences. This legacy kernel score is a ranking aid, not proof of a solution.

The GUI ranks received candidates on the English trigram scale in every mode.
Its Stop confidence control defaults to 80, with 0 disabling automatic stopping.
`SearchSpec.stop_confidence` captures the target for each run. The worker callback
rates a candidate on that same scale, retains it even if the output queue is full,
then cancels the pool when the target is reached. `SearchSnapshot.reached_confidence`
distinguishes this from manual cancellation or exhaustion. The GUI drains the remaining
queue, selects the highest-scoring received answer and copies its text to Candidate
decryption. No hidden challenge data is used. Core callers retain the previous behavior
unless they set a target. Scenario files store the target as `Message.stop_confidence`;
older files use 80.

All six pages use compact controls. Toolbars pack controls at their natural widths
and wrap when needed, rather than reserving empty grid columns. Diagrams use the
remaining height while preserving their aspect ratio. The message editors share a row, and the Bombe
uses a resizable split between worker animations and candidates. Those panes scroll
independently when their contents exceed the available space. Page scrolling remains
available for small windows and expanded help. The layout test checks that all six
pages fit at 1366 by 768 and 1900 by 1000 with help collapsed and the tutorial
closed. It also checks keyboard traversal and scaled plugboard hit testing.

The `SearchSpec` interface contains ciphertext, crib/menu, public rings/reflector and search controls. It has no hidden plaintext or secret-key field. The GUI never compares candidates with the hidden plaintext or generator key. All candidate ranking uses decrypted-text statistics alone. An equivalent recovered key may produce the same plaintext.

### Search modes

| Mode | Known | Searched | Rotor states |
| --- | --- | --- | ---: |
| Training / modern accelerated | Rings, reflector, chosen crib alignment | Rotor order, start windows; plugboard by constraints | 1,054,560 |
| Training / historical display | Same | Same kernel, slower rack movement | 1,054,560 |
| Advanced / unknown rings | Reflector, chosen crib alignment | All training dimensions plus all rings | 18,534,946,560 |

Advanced mode is implemented but can take a long time. Some ring/start combinations are equivalent for a particular message. It searches these redundancies rather than claiming a uniquely identifiable secret key. The reflector is always a supplied assumption.

A stop is not automatically a full solution. Unresolved letters use identity only for the preview; unobserved plugboard pairs may change the rest of the plaintext. This release does not guess all remaining cables with a secondary language-driven search.

**Stops/state** defaults to 64 to bound output from weak menus. Zero removes that limit. Reaching a limit increments **Stop-capped states**; those states may have additional omitted solutions. Use a stronger crib or unlimited enumeration when completeness matters. The rotor-state counter still counts tested rotor settings, not the number of plugboard hypotheses.

## Parallelism and measurements

The search owns a persistent pthread pool, reused until the user changes worker count. Workers pull 64-state chunks using an atomic work index. Each worker keeps local counters and publishes a mutex-protected snapshot once per chunk. Cache-aligned worker records reduce false sharing. Completed-order counts are aggregated by chunk. There is no mutex in the per-letter cipher operation.

Pause parks workers on a condition variable after their current work unit. Resume wakes them. Stop sets an atomic cancellation flag checked between states, in crib-offset stepping and in constraint recursion. Search shutdown joins threads only after requesting cancellation. An especially ambiguous state may delay pause; stop remains cancellable inside the solver.

GTK samples snapshots at 10 Hz. Throughput samples are taken about five times per second and the rolling graph stores 240 samples. Enigma uses GTK frame-clock callbacks. Workers do not access widgets, sleep for animations or emit a frame per searched state. One virtual rack represents one CPU worker for education; it does not represent one physical wartime Bombe.

The dashboard reports tested/remaining states, percentage, elapsed wall time, current and average states/sec, contradictions, crib-consistent stops, completed rotor orders, worker count and output limits. Paused time is included in elapsed wall time. Current throughput falls to zero on pause/completion. No historical speed comparison is asserted.

The candidate queue holds 256 entries. The GUI drains at most 32 each refresh and retains up to 2,000 score-ranked rows. Overflow is counted and displayed; the solver continues. Thus a weak menu can produce more genuine stops than the UI retains. Strengthen the crib to inspect a manageable set. Full candidate export is future work.

### English detective search without a crib

Mode 3 uses `blind_spec_init` instead of menu construction. Its inputs are normalized
ciphertext, public rings/reflector, an attempt count and a random seed. It accepts
50 to 2,047 normalized letters within the existing 2,047-byte raw input limit.
The worker pool enumerates all training rotor states in single-state chunks.
For each state, `blind_test_state` precomputes the rotor permutation at each
message position. Plugboard hill climbing first maximizes index of coincidence,
then the bundled English trigram score. Each phase permits up to 20 improving
steps; there is no exhaustive plugboard search. Restarts are bounded to 1–16.

Moves add/remove pairs and reconnect contacts while preserving an involution with
at most ten pairs. Cancellation is checked while building maps and between move
groups. Pause takes effect after the current rotor state. Workers publish only
new global best English scores; a full queue discards the oldest guess to retain
the new best. Candidate rows show a heuristic English confidence percentage, in all modes.
No candidate is compared against a stored original or declared correct automatically.

See [language-model.md](language-model.md) for the scoring corpus, generation
procedure and its limitations. The deterministic tests cover an original 543-letter
English message, ten plugs, nonzero rings and reflector C; a 24-state worker-pool
search recovers its key. The GUI test removes the Challenge before starting and
checks recovery with an empty crib and invalid ignored machine controls. These are
regression examples, not a calibrated population success rate or 80% confidence
claim. The supplied demo has an early correct state for a quick first result.

The design follows the general ciphertext-only IC/hill-climbing approach discussed
by Ostwald and Weierud in [Modern Breaking of Enigma Ciphertexts](https://www.cryptocellar.org/pubs/enigma-modern-breaking.pdf).
This implementation does not reproduce their full algorithm or inherit their
published recovery results.

### CPU benchmark

**Benchmark CPU**, the benchmark tab, and `--benchmark` run the same scalar menu kernel on a fixed 65,536-state workload. Each row repeats that workload for at least 0.75 seconds, using 1, 2, 4, 8 and further powers of two, plus the logical CPU count when it is not a power of two.

A background coordinator waits on the pool's completion condition rather than throttling workers with timer polling. `CLOCK_MONOTONIC` measures actual tested states over elapsed time. Pool construction is excluded; dispatch, completion and aggregation costs are included. Speedup is relative to one thread and efficiency is speedup divided by thread count. The graph uses measured rows. Searches and benchmarks are mutually exclusive in the UI. Results depend on the menu, CPU, compiler, thermal conditions and competing workloads.

## Save and load

Enter a filename or full path on the message desk and use **Save scenario** or **Load scenario**.

Scenario imports are limited to 64 KiB, including comments and unused keys. Both startup and the Load button enforce this limit while reading, before INI parsing. A rejected import leaves the current scenario unchanged.

The INI format uses GLib KeyFile:

- `[Machine]`: rotor numbers from left to right, three-letter rings/windows, plugboard pairs and reflector.
- `[Message]`: plaintext, ciphertext, crib, normalized offset, mode, random-ring preference and stop limit.
- Optional `[Challenge]`: the separately stored generator key and hidden plaintext for later reveal and validation.

Saved challenges contain the secret in plain text. Hiding it in the UI does not remove it from the saved file. For a shareable ciphertext-only scenario, remove `[Challenge]` and clear `[Message]` plaintext. Active worker state, pause state and candidate queues are never saved. Invalid keys, excessive lengths and inconsistent generated challenges are rejected without replacing the current scenario.

## Tests and validation

```sh
meson test -C build --print-errorlogs
```

The tests cover rotor and inverse permutations, reflector and plugboard involutions, duplicate plugboard rejection, zero/ten-pair cases, no-self-encryption, reciprocal operation, ring settings and double stepping. The required vector is checked directly:

```text
I II III / reflector B / rings AAA / windows AAA / no plugboard
AAAAA -> BDZGO
```

The deterministic Bombe integration test generates ciphertext with a fixed ten-pair plugboard, gives the solver only ciphertext and a crib, and checks the expected order/windows and full plaintext among the stops. It also exercises a nonzero crib offset, nonzero rings, reflector C, advanced-state decoding, parallel work, pause/resume, prompt cancellation, pool reuse and 1/2/4-thread benchmark rows.

On a running desktop, the optional GUI smoke checks call the actual callbacks, run a full example search, inspect the candidate and close automatically:

```sh
G_DEBUG=fatal-warnings ./build/enigma-bombe-lab --threads 4 --smoke-test 15000
G_DEBUG=fatal-warnings ./build/enigma-bombe-lab --benchmark --smoke-test 15000
```

The hidden `--smoke-test` argument is a timeout in milliseconds. Increase it on slow or instrumented machines. It tests keyboard encryption, reciprocity, plugboard edits, randomization, hiding/revealing, save/load round-trips, invalid-file rejection, automatic alignment and recovery. The benchmark variant checks the GUI benchmark output. Core tests remain display-independent.

An instrumented build:

```sh
meson setup build-asan --buildtype=debugoptimized -Db_sanitize=address,undefined -Db_lundef=false
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

GCC and Clang release builds, the three core tests, both GUI smoke workflows, and ASan/UBSan core tests were exercised on Linux with GTK 4.22. No distro package-manager installation was needed on the development machine; the package commands above name the corresponding distribution dependencies.

## Source tree

```text
.
├── .clang-format
├── .gitignore
├── meson.build
├── README.md
├── examples/
│   └── training.ini
├── resources/
│   ├── lab.gresource.xml
│   └── style.css
├── src/
│   ├── main.c
│   ├── app.c              app.h
│   ├── gui.c              gui.h
│   ├── rotor.c            rotor.h
│   ├── plugboard.c        plugboard.h
│   ├── enigma.c           enigma.h
│   ├── crib.c             crib.h
│   ├── menu.c             menu.h
│   ├── bombe.c            bombe.h
│   ├── search_worker.c    search_worker.h
│   ├── benchmark.c        benchmark.h
│   ├── enigma_view.c      enigma_view.h
│   ├── bombe_view.c       bombe_view.h
│   └── menu_view.c        menu_view.h
└── tests/
    ├── test_enigma.c
    ├── test_menu.c
    ├── test_bombe.c
    └── test_tutorial.c
```

`rotor`, `plugboard` and `enigma` implement the machine. `crib` and `menu` build constraints; `bombe` solves one state. `search_worker` schedules work and publishes results. `benchmark` owns its background coordinator. `app` handles workflow and persistence, `gui` builds controls, and the three view modules draw Cairo visualizations.

## Historical scope and limits

The stepping implementation tests notches at the pre-keypress window letters, steps the required rotors, then applies electrical transformations with offset `window - ring`. The notch belongs to the alphabet ring, so changing Ringstellung does not change its visible turnover letter. The identity entry wheel, rotor wirings and B/C reflectors match the three-moving-rotor service machine described in [Crypto Museum's working explanation](https://www.cryptomuseum.com/crypto/enigma/working.htm) and [wiring tables](https://www.cryptomuseum.com/crypto/enigma/wiring.htm). The double-step sequence is also documented in [David Hamer's technical note](https://www.cryptomuseum.com/people/hamer/files/double_stepping.pdf).

The menu solver is inspired by the [British Bombe](https://www.cryptomuseum.com/crypto/bombe/index.htm). It does not reproduce a physical diagonal board, drum wiring cabinet, indicator procedure or wartime operating schedule. Its racks are modern educational drawings.

Current limits:

- Rotors I-V only. Naval VI-VIII, thin reflectors, M4, alternate entry wheels and historical indicator procedures are absent.
- Messages are limited to 2,047 input bytes, with a 256-letter crib. The GUI requires at least eight crib letters for a search. Very weak menus can still be expensive.
- Search uses one alignment and a known reflector per run. Ring search is exhaustive and redundant rather than optimized for equivalent keys.
- No secondary completion of plugboard pairs absent from the menu. A short crib does not guarantee readable plaintext or a unique key.
- Stop limits, queue capacity and displayed-row limits can omit candidate details. The dashboard exposes these limits; choose unlimited stops and stronger cribs when appropriate.
- No saved search checkpoints or candidate export. The simple English/German score is not a reliable language detector.
- The engine is scalar CPU code. It makes no promise of millions of states per second on every CPU or for every crib.

Useful next work includes Naval M4 and VI-VIII support, stronger menu ordering, lazy scrambling-table construction, secondary constrained plugboard completion, automatic multi-alignment runs, candidate export, ring-equivalence reduction, resumable work units, and measured SIMD or GPU/OpenCL acceleration with the scalar implementation retained as a correctness reference.

## Guided tutorial and screenshots

`--tutorial` opens the same walkthrough as the Tutorial header button. Its actions use the normal encryption and search callbacks. Hide as intercept checks that the visible ciphertext matches the current settings before moving the key and plaintext into challenge storage.

The GTK integration test clicks through the lesson, rejects changes to the known phrase or key, and verifies actual recovery using the worker pool. It returns skip status 77 without a graphical display.

```sh
meson test -C build --suite gui --print-errorlogs
LAB_SCREENSHOT_DIR=docs/screenshots ./build/test_tutorial
LAB_TEST_SMALL_WINDOW=1 ./build/test_tutorial
```

Screenshots are direct GTK renders of the app during that test. The test uses a fixed random seed for reproducibility. Real tutorial sessions use fresh random settings.
