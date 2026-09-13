# Monix

Monix is a native Windows system monitor with retro terminal-inspired rendering, telemetry collection, shader/runtime diagnostics, and SCRAM risk summaries.

## Current Layout

- `src/core`: reusable monitoring, events, validation, collectors, security, and platform abstractions.
- `src/native`: Windows app entry point, UI, rendering, settings, telemetry adapters, and native test unity files.
- `tests`: subsystem tests for the core collectors and event pipeline.
- `Monix/Monix`: legacy/runtime asset tree used by the current build.
- `docs`: refactor notes and project conventions.

See `docs/refactor-plan.md` and `docs/naming-conventions.md` before moving files so build references stay in sync.
