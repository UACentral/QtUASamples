# Copilot instructions

## Building qtcon-ua-sub

- Build with `qtcon-ua-sub\build.cmd` (run from that folder as `.\build.cmd`; `.\build.cmd clean` for a clean build). Don't invoke cmake manually.
- Output: `qtcon-ua-sub\build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug\qtcon-ua-sub.exe` (git-ignored). This is the same Debug build tree Qt Creator uses (MinGW Makefiles generator). Reuse it; don't rebuild or delete it just to run the app.
- Toolchain defaults to Qt 6.12.0 llvm-mingw under `C:\Qt`; override with `QT_DIR` / `MINGW_DIR`.

## Running qtcon-ua-sub

- Usage: `qtcon-ua-sub.exe [endpointUrl] [nodeId]`.
- Qt debug output is not shown when stderr is redirected unless `QT_ASSUME_STDERR_HAS_CONSOLE=1` is set.
- The client only supports SecurityMode None / SecurityPolicy None.
