# Repository Guidelines

## Project Structure & Module Organization
- Application samples live under `apps/` (for example `apps/esp32s3_demo`).
- Shared code is in `boards/`, `drivers/`, `include/`, `lib/`, and `dts/`.
- Tests are in `tests/`; documentation and design notes are in `doc/`.
- `scripts/` holds helper utilities; `build/` is generated output and should not be committed.

## Build, Test, and Development Commands
- Configure and build an app (from this directory):  
  `west build -b esp32s3_devkitc apps/esp32s3_demo`
- Rebuild after changes:  
  `west build -t rebuild`
- Flash to hardware:  
  `west flash`
- Run Zephyr test suites:  
  `west twister -T tests`

## Coding Style & Naming Conventions
- Follow upstream Zephyr C style: 4-space indentation, no trailing whitespace.
- Use `snake_case` for functions and variables, `ALL_CAPS` for macros, and `PascalCase` for types/structs.
- Keep modules small and cohesive; prefer splitting into `*.c` and matching `*.h` under `include/`.
- When adding new apps, mirror existing directory layout in `apps/<board>_<feature>/`.

## Testing Guidelines
- Add or update tests in `tests/` whenever adding features or fixing bugs.
- Name test files by feature (for example `test_wifi_scan.c`).
- Ensure new tests run with `west twister -T tests` and avoid board-specific assumptions where possible.

## Commit & Pull Request Guidelines
- Use imperative, concise commit messages (for example `Add Wi-Fi scan helper`).
- Reference related issues using `#<id>` in the body when applicable.
- For pull requests, describe the change, affected boards/apps, and how it was tested; include logs or screenshots for user-visible changes.

## Agent-Specific Instructions
- Keep changes minimal, focused, and consistent with existing patterns.
- Prefer small, reviewable patches; update or add tests alongside code changes.
- Do not modify licensing, project layout, or CI workflows without explicit instruction.
