# Enigma Bombe Lab

Encrypt a message with an Enigma machine, then let your PC try to crack it. This is a Linux desktop app. Everything runs locally.

## Set up your PC

You need a Linux desktop and GTK 4.8 or newer. Install the build tools for your distribution.

**Ubuntu / Debian**

```sh
sudo apt update
sudo apt install build-essential meson ninja-build pkg-config libgtk-4-dev
```

**Fedora**

```sh
sudo dnf install gcc meson ninja-build pkgconf-pkg-config gtk4-devel
```

**Arch Linux**

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf gtk4
```

Open a terminal in this project's folder, then build and start the tutorial:

```sh
meson setup build --buildtype=release
meson compile -C build
./build/enigma-bombe-lab --tutorial
```

If `build` already exists, skip the setup command and run the compile command again. Building and running do not need `sudo`.

## Use the app

### Start with the guided tutorial

Click **Tutorial** in the top-right corner, or launch with `--tutorial`. Follow the gold button in the tutorial panel. It takes you through nine steps in the actual app, from choosing a key to inspecting a candidate answer.

Starting the lesson replaces your current message and settings. Opening the tutorial does not. You can exit at any time and keep working with the current message.

![The built-in tutorial above the Enigma controls](docs/screenshots/tutorial.png)

### 1. Scramble the machine

On **ENIGMA**, click **Randomize machine**. This chooses the rotor order, starting letters and ten plugboard pairs. Leave **Randomize rings too** off for your first attempts, with Ringstellung **AAA** and reflector **B**.

On **PLUGBOARD**, the cables swap pairs of letters. You can click two sockets to connect them, click a connected socket to remove its cable, or use **Randomize 10 pairs**.

![The plugboard and its letter-swapping cables](docs/screenshots/plugboard.png)

### 2. Encrypt your message

Open **MESSAGE / INTERCEPT** and enter text in **Plaintext**, then click **Encrypt**. The ciphertext appears underneath. Only A-Z letters are kept; spaces and punctuation are removed. Open **ENIGMA** to watch the drums and lamps explain the encryption.

For the tutorial, keep its supplied first line and write your own final sentence. That first line gives the PC a strong clue for its first search.

![A practice message and its encrypted ciphertext](docs/screenshots/message.png)

Click **Hide as intercept** to hide this message and its key while preserving the ciphertext. The PC will try to recover it.

**Create random intercept** is different: it chooses a new key and encrypts your text again. Use **Hide as intercept** when you want to crack the exact message you just encrypted.

### 3. Give the PC a clue

Open **CRIB / MENU**. A *crib* is a phrase you think appears in the original message. Training and advanced modes need this clue. The no-crib mode below can attempt a search without it.

Enter the phrase and its **Offset**. Offset **0** means the phrase starts at the first letter, **1** at the second letter, and so on. Count letters after spaces and punctuation have been removed. The tutorial fills in its known first line at offset 0.

![The practice crib, its offset and the resulting menu graph](docs/screenshots/crib.png)

If you do not know the offset, use **Auto choose promising alignment** or try **Previous alignment** and **Next alignment**. Automatic alignment picks a promising position, not a guaranteed correct one. An alignment is impossible if any crib letter matches the ciphertext letter at the same position.

### 4. Start the search

Open **BOMBE**, choose **Training / modern accelerated**, and select **4** CPU workers to start. **All CPUs** uses all available logical CPUs, which may make the rest of your computer less responsive.

Click **START BOMBE**. Watch the progress and scroll down for the worker drums and **BOMBE STOPS**. **Pause**, **Resume** and **Stop** control the search.

![The real Bombe search running during the tutorial](docs/screenshots/bombe.png)

Training mode knows the rings and reflector, and searches rotor order, starting letters and plugboard connections. It does not receive your hidden message or secret key.

### 5. Read the result

When candidates appear, double-click a row under **BOMBE STOPS**, or select it and press Enter. The tutorial also provides **Inspect best candidate** after the search finishes.

Read **Candidate decryption** on **MESSAGE / INTERCEPT**. The candidate's settings are loaded into **ENIGMA** and **PLUGBOARD**. Use **Reveal secret key** afterward to compare with the original.

![Inspecting the independently recovered practice message](docs/screenshots/recovered.png)

A stop means the candidate fits your clue. It may still be wrong. If the answer is incomplete or there are no stops, check the crib, offset, rings and reflector. Try a longer crib. The tutorial uses a deliberately long clue that covers the alphabet; a short real-world guess is harder.

### Try without a crib

Start with this ready-made English example:

```sh
./build/enigma-bombe-lab --threads 4 examples/no-crib.ini
```

On **BOMBE**, select **No crib / English detective** and click **START BOMBE**.
Leave the crib empty. Double-click the highest-scoring row to read its decryption.
Use **Stop** when you have a candidate to inspect. This example deliberately puts
the correct rotor state early in the search so you can try the workflow quickly.
It is not a timing benchmark for arbitrary messages.

For your own message, encrypt English text, choose **Hide as intercept**, then use
the same no-crib mode. It uses ciphertext and the supplied **rings and reflector**.
It ignores the crib, rotor order, starting-window and plugboard controls, and never
receives the hidden message or key. You can also paste ciphertext from elsewhere
and set the known rings and reflector on **ENIGMA**.

The search tries all 1,054,560 rotor states and improves plugboard guesses using
English letter statistics. **No-crib attempts per rotor state** controls how many
starting plugboards it tries. The first is empty; extra attempts use random plugs.
More attempts take longer. A full run can take hours; Pause, Resume and Stop work
while it runs. Results show improving guesses as they arrive.

The minimum accepted input is **50 letters** after removing spaces and punctuation.
For a first puzzle, use **300–500 letters of ordinary English**. This is a
practical starting point, not an 80% confidence threshold. Short, repetitive,
non-English or unusual text can produce convincing wrong answers. Even completing
the rotor scan does not exhaust every plugboard.

Every candidate shows **English confidence**, a heuristic percentage calculated
only from its decrypted text. Higher values rank first. The app never compares
candidates against the stored original, even for demos or self-made challenges.
Read the full candidate message and decide for yourself.

The percentage measures English-language fit on a fixed display scale. It is not
a calibrated probability of recovering the correct message or settings. Hover a
row for the underlying score. See [the scale definition](docs/language-model.md).
The percentage is a guide to English resemblance.

### Come back later

On **MESSAGE / INTERCEPT**, enter a filename such as `my-message.ini` and click **Save scenario**. Use **Load scenario** to reopen it. Saved scenarios include the original key and plaintext, so they are not secret-free challenge files.

Useful launch commands:

```sh
# Open the app normally
./build/enigma-bombe-lab

# Repeat the tutorial
./build/enigma-bombe-lab --tutorial

# Open a ready-made challenge, then press START BOMBE
./build/enigma-bombe-lab --threads 4 examples/training.ini

# Measure your CPU's search speed
./build/enigma-bombe-lab --benchmark
```

To check the build:

```sh
meson test -C build --print-errorlogs
```

The tutorial test opens a window and runs a real recovery. It skips itself when no graphical display is available. Technical details, solver limitations and development notes are in [docs/development.md](docs/development.md).
