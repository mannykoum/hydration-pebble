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
- 4 watch views with directional navigation:
  - **Down** from Main → Amount list; **Up** from Main → Detail plot → Weekly chart.
  - In the Amount list, **Up** from the first row returns to Main; **Down** at the last row does nothing (no wrap).
  1. Main progress view (intake and day-progress bars) with long-select goal editing.
  2. Amount list with configurable add/remove entries and cup icon.
  3. Daily cumulative progress plot with selectable previous days.
  4. Last-7-days bar chart view.
- Phone configuration for unit (ml/cups/pints), goal, and 4 signed amount presets.
- Companion intake log persistence in phone local storage for later sync/export work.
- Color-aware UI theme on color Pebble platforms with monochrome fallback.
- Lightweight pulse animation on highlighted UI elements and goal-complete messaging.

## Branch strategy
- `main`: release/version bumps only.
- `dev`: default integration branch for validated feature work.
- `feature/*`: short-lived branches created from `dev` and merged back via pull request.
