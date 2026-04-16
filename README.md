# hydration-pebble
A pebble app to track water intake on a daily basis.

## Setup
1. Install Pebble SDK: https://developer.repebble.com/sdk/
2. Build from this repository root:
   ```bash
   pebble build
   ```
3. Install to emulator/device:
   ```bash
   pebble install --emulator basalt
   ```

## Features
- 4 watch views with wrap-around navigation:
  1. Main progress view (intake and day-progress bars) with long-select goal editing.
  2. Amount list with configurable add/remove entries and cup icon.
  3. Daily cumulative progress plot with selectable previous days.
  4. Last-7-days bar chart view.
- Phone configuration for unit (ml/cups/pints), goal, and 4 signed amount presets.
