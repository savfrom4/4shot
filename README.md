# 4shot

**Small** X11 utility used for taking screenshots.

The only dependencies of `4shot` are **XLib** and **libpng**.

## Modes
- `--full` Just uses fullscreen.
- `--rect` Prompts user to select an area.
    - `LMB` select an area
    - `RMB`/`Enter` take a screenshot
    - `ESC` cancel

## Output
- `--stdout` Outputs PNG image to `stdout`.
- `--file [filename]` Outputs PNG image to a file `filename`.
