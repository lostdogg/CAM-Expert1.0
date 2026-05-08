# Contributing to CAM-Expert

Thanks for contributing to CAM-Expert.

## Supported platforms

- **Primary target:** Windows 10/11 (Win32 + OpenGL runtime).
- **Toolchains:** Visual Studio 2019/2022/2026 or MinGW-w64 on Windows.
- **Linux/container environments:** useful for configure/build reproducibility checks, but native Linux compile is expected to fail at `windows.h` without a Windows-targeting toolchain.

## Build commands

Use one of the following flows from the repository root.

### Visual Studio

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Presets (Windows hosts)

```powershell
cmake --list-presets
cmake --preset windows-vs2026
cmake --build --preset release-vs2026
```

### MinGW on Windows

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Containerized configure/build smoke

```bash
docker build -t camexpert-ci .
docker run --rm camexpert-ci
```

## Coding style

- Keep edits scoped and module-local.
- Follow the existing style in touched files (naming, formatting, and structure).
- Keep Win32 command wiring in `MainWindow` synchronized with UI labels and behavior.
- Update docs when changing command IDs, workflows, presets, or module layout.

## Testing expectations

There is currently no automated unit-test suite in this repository. Validate changes with:

1. CMake configure smoke test.
2. Build smoke test in a Windows toolchain (or document expected non-Windows failure context).
3. Manual smoke run of the changed UI/CAM workflow.
4. CAM generation and post-output sanity checks when toolpath/post logic is affected.
5. Verify/backplot/machine simulation smoke checks for motion-related changes.

## Pull request checklist

- [ ] Scope is focused and limited to the intended change.
- [ ] Configure/build behavior was validated for the target environment.
- [ ] Manual smoke checks cover the changed workflow.
- [ ] Documentation was updated for user-visible changes.
- [ ] Command IDs, UI labels, and wired actions remain consistent (if touched).
