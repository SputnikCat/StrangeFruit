# Strange Fruit

Prime numbers on a mid-80s vector phosphor display, built with [raylib](https://www.raylib.com).
Part scientific instrument, part 1986 vapor dream. The screen is portrait (1080x1920) so it
captures straight into phone-shaped video.

## Module 01: Ulam spiral

The integers are laid out on a square spiral starting from 1 at the centre; primes light up.
Prime-rich diagonals appear out of nowhere, which is the whole mystery of the Ulam spiral.

Two ways of finding the primes, both computed live:

| Mode | What it does |
|---|---|
| **1 Trial division** | Brute force. Every n is divided by 2, 3, 5, 7 ... up to floor(sqrt(n)). No divisor means prime. The spiral grows outward as it goes and the view zooms out to follow. |
| **2 Sieve of Eratosthenes** | The whole range starts as candidates. For each prime p, multiples from p^2 are crossed out (they flash and vanish). When p comes up, everything below p^2 is confirmed prime at once, so primes arrive in bursts. |

Speed starts slow enough to read every division, then doubles every couple of seconds
(AUTO). When a run finishes, the result holds for a few seconds and the other mode starts
(CYCLE).

The screen:

- **Ulam spiral**: the plot. While the cells are big you see the numbers and the spiral path.
- **Readout**: current n and its spiral coordinates, pi(n), prime density, widest prime gap, twin primes.
- **Math terminal**: the equation being worked right now, and a log of results.
- **Primes / sec**: how fast primes are being found.
- **pi(n) vs n / ln n**: the prime counting function against Gauss's estimate, and their ratio.

## Controls

| Key | Action |
|---|---|
| 1 / 2 | Restart in trial division / sieve mode |
| Up / Down | Double / halve the speed (turns AUTO off) |
| A | AUTO speed ramp on/off |
| [ / ] | Spiral size: 101, 201, 301 or 401 cells across |
| Space | Pause |
| R | Restart |
| C | Auto-cycle between modes on/off |
| N | Next music track |
| M | Mute |
| H | Hide the key help (clean capture) |
| F11 | Fullscreen |
| Esc | Quit |

Command line: `--sieve` starts in sieve mode, `--rate OPS` sets a fixed speed, `--fast` runs
flat out, `--fullscreen`, and `--shot SECONDS FILE.png` saves the screen and quits.

## Music

Drop `.mp3`, `.wav` or `.ogg` files into `soundtrack/`. A random track plays; when it ends,
the folder is rescanned and another random one starts.

## Building

Windows, Visual Studio 2026: run `build-VisualStudio2026.bat` (it regenerates the project with
premake, which picks up every file in `src/`), then open `StrangeFruit.slnx`.
MinGW: `build-MinGW-W64.bat`, then `make` in `build/`.

The vector look (beams, phosphor persistence and bloom, stroke font) is shared with
VectorBricks, DiarrheaMan and StrangePhysics.
