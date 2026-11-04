# INDIKAT Deep Space

An algorithmic reverb that runs in the browser, in the INDIKAT plugin look.

Open `index.html` in a browser, press **Play** and turn the knobs. You can also drop your own audio file onto the unit.

## Controls

| Group | Knobs |
|---|---|
| — | Mix, Predelay |
| Decay | RT60, 0.2–30 s (big dial) |
| Damping | High freq, High shelf, Bass freq, Bass mult |
| Shape | Size, Attack |
| Diff | Early, Late |
| Mod | Rate, Depth |
| EQ | High cut, Low cut |

Footer: **Mode** (Deep Hall, Room, Chamber, Plate, Nebula, Ambience), **Color** (Vintage, Classic, Modern), 12 **Presets**, and a **Test** source (Keys, Pluck, Drums, Voice or your own file).

Drag a knob up or down. Hold Shift for fine moves, double-click to reset, or use the mouse wheel or arrow keys.

## How it works

Everything is Web Audio, with no libraries:

- Predelay, then 4 Schroeder allpasses per side for early diffusion and 2 longer ones for attack.
- An 8-line feedback delay network mixed by a Hadamard matrix. The feedback of each line is set from Decay, so the tail hits −60 dB at the chosen time.
- In each loop: a high shelf (High freq / High shelf), a low shelf that stretches the bass decay (Bass freq / Bass mult), a Color low-pass, and a soft limiter.
- Each line's delay is modulated by its own LFO (Rate / Depth).
- Late diffusion sits on the tank output. Nesting allpasses inside a Web Audio feedback loop makes them unstable.
- The output goes through the High cut and Low cut filters, then is mixed with the dry signal.
