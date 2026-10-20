# Dynamic

A Geode mod prototype for Geometry Dash 2.2081 with a timing-window jump assist.

Features:
- master enable/disable
- menu hotkey: 7
- activation point list
- early and late tolerance configuration
- debug display
- modular logic and menu code

Important:
This project is intentionally conservative and is meant as a repo-ready mod scaffold. It does not globally alter physics, speed, or every jump in Geometry Dash. It is built to be extended with a version-specific GD 2.2081 input/collision hook for real gameplay validation.

Build instructions:
1. Install the Geode SDK.
2. Clone this repo.
3. Set GEODE_SDK_PATH to the Geode SDK directory.
4. Run:
   ```bash
   cmake -B build -S . -DGEODE_SDK_PATH=/path/to/geode-sdk
   cmake --build build --config Release