# Building qtopcua against open62541 v1.5.10 for wuac

How `wuac` was brought up on Qt 6.12.0 with a separately built open62541 v1.5.10
(Windows, llvm-mingw). Qt 6.12.0 normally bundles open62541 v1.5.7.

## Layout

| Item | Path |
|------|------|
| Qt kit | `C:\Qt\6.12.0\llvm-mingw_64` |
| Toolchain | `C:\Qt\Tools\llvm-mingw2217_64` (earlier Qt versions used `llvm-mingw1706_64`) |
| CMake / Ninja | `C:\Qt\Tools\CMake_64`, `C:\Qt\Tools\Ninja` |
| qtopcua worktree (tag `v6.12.0`) | `C:\g\qtopcua-6.12.0` |
| open62541 v1.5.10 source | `C:\g\open62541-1.5.10` |
| open62541 install prefix | `C:\Qt\open62541-install` |

## Rules learned

- qtopcua must match the installed Qt version exactly (`6.12.1` branch will not
  configure against Qt 6.12.0). Use the matching tag, e.g. `git worktree add --detach <dir> v6.12.0`.
- Use the compiler that ships with the Qt kit. Mixing GCC with an llvm-mingw kit
  fails at link time. Start from a clean build directory when switching.
- Qt 6.9.x+ here only ships `llvm-mingw_64` kits (no `mingw_64`).
- open62541 must be built with the same compiler as Qt.

## 1. Build and install open62541 v1.5.10

```powershell
$env:PATH="C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\llvm-mingw2217_64\bin;$env:PATH"
git clone --branch v1.5.10 --depth 1 --recurse-submodules --shallow-submodules `
  https://github.com/open62541/open62541 C:\g\open62541-1.5.10
cmake -S C:\g\open62541-1.5.10 -B C:\g\open62541-1.5.10-build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON `
  -DUA_NAMESPACE_ZERO=FULL -DUA_ENABLE_AMALGAMATION=OFF `
  -DCMAKE_INSTALL_PREFIX=C:/Qt/open62541-install
cmake --build C:\g\open62541-1.5.10-build --parallel
cmake --install C:\g\open62541-1.5.10-build
```

`UA_NAMESPACE_ZERO=FULL` is required; qtopcua relies on full namespace 0.

## 2. Build and install qtopcua

```powershell
$env:PATH="C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\llvm-mingw2217_64\bin;C:\Qt\6.12.0\llvm-mingw_64\bin;C:\Qt\open62541-install\bin;$env:PATH"
cd C:\g\qtopcua
git worktree add --detach C:\g\qtopcua-6.12.0 v6.12.0
qt-cmake -S C:\g\qtopcua-6.12.0 -B C:\g\qtopcua-build -G Ninja `
  -DINPUT_open62541=system `
  -Dopen62541_DIR=C:/Qt/open62541-install/lib/cmake/open62541
cmake --build C:\g\qtopcua-build --parallel
cmake --install C:\g\qtopcua-build
Copy-Item C:\Qt\open62541-install\bin\libopen62541.dll C:\Qt\6.12.0\llvm-mingw_64\bin\
```

Notes:

- The switch is `-DINPUT_open62541=system`. `-DFEATURE_system_open62541=ON` alone
  fails with `open62541_FOUND` empty, because the package lookup only runs when
  the input is `system`.
- `qt-configure-module.bat ... -DCMAKE_PREFIX_PATH=C:/...` mangles the drive
  letter, so use `qt-cmake` with `-Dopen62541_DIR` instead.
- With the split system headers, `qopen62541valueconverter.cpp` fails with
  `undeclared identifier 'UA_ObjectAttributes_default'`. Fix (in the worktree):
  add `#include <open62541/util.h>` after `#include <open62541/types.h>` inside
  the `#ifdef USE_SYSTEM_OPEN62541` block.
- `libopen62541.dll` must be findable at runtime (Qt `bin`, `PATH`, or next to
  `wuac.exe`).
- To use the bundled copy instead, run `qt-configure-module.bat C:\g\qtopcua`
  with no extra arguments (open62541 backend, no security support).

## 3. Build and test wuac

```powershell
cd C:\g\QtUASamples\wuac
cmake -S . -B C:\g\wuac-build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/llvm-mingw_64
cmake --build C:\g\wuac-build
```

Or open `CMakeLists.txt` in Qt Creator with the Qt 6.12.0 `llvm-mingw_64` kit.
Verify the plugin uses the new library:

```powershell
llvm-objdump -p C:\Qt\6.12.0\llvm-mingw_64\plugins\opcua\open62541_backend.dll | Select-String 'DLL Name'
```

`libopen62541.dll` should appear in the list.

## Changes made to wuac

- `CMakeLists.txt` is Qt 6 only: `find_package(Qt6 REQUIRED COMPONENTS Widgets OpcUa Charts)`.
- `mainwindow.cpp`: connects only to an endpoint with SecurityPolicy `None` and
  SecurityMode `None` (these builds have no security support), shows an error and
  resets the Connect button if none exists, and retries a failed connect up to 3
  times, 1 second apart, since the test server (`m3:48400`) sometimes timed out.

## Version history tried

| Qt | qtopcua | Bundled open62541 | Kit / toolchain |
|----|---------|-------------------|-----------------|
| 6.9.2 | v6.9.2 | 1.4.9 | `mingw_64` / mingw1310 |
| 6.9.3 | v6.9.3 | 1.4.9 | `llvm-mingw_64` / llvm-mingw1706 |
| 6.10.3 | v6.10.3 | 1.4.13 | `llvm-mingw_64` / llvm-mingw1706 |
| 6.11.1 | v6.11.1 | 1.4.14 | `llvm-mingw_64` / llvm-mingw1706 |
| 6.12.0 | v6.12.0 | 1.5.7 (replaced by system 1.5.10) | `llvm-mingw_64` / llvm-mingw2217 |
