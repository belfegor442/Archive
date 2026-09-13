# Naming Conventions

This project is mid-refactor, so names should make migration safer instead of hiding history.

## Source Files

- Use `PascalCase.hpp` and `PascalCase.cpp` for C++ modules that expose a domain object or subsystem.
- Use lower-case resource names for shaders, presets, fonts, logs, and generated data.
- Avoid date, phase, or temporary words in durable source names: `fase13_validation_test_unity.cpp` should become a descriptive test target name when its build references are updated.
- Keep compatibility wrappers only when they are listed in the build scripts or consumed by external tooling.

## Directories

- `src/core`: platform-neutral event, validation, collector, and integration logic.
- `src/native`: Windows application, rendering, telemetry adapters, and UI.
- `tests`: test entry points grouped by subsystem.
- `Monix/Monix`: legacy/runtime asset tree. New source should move toward `src` unless it is a bundled asset or third-party runtime fixture.
- `original`: historical references only. Do not include it in production builds.

## Generated Files

Object files, screenshots, logs, shader caches, and ad-hoc test output belong under ignored build or runtime directories. They should not sit at repository root.
