# Building Qt OPC UA with the bundled open62541 backend

How to build and install the stock `qtopcua` module (tag `v6.12.0`) into an existing Qt 6.12.0 kit,
using the open62541 that ships inside the qtopcua sources (`src\3rdparty\open62541`).
This is the default build: no separate open62541 library is needed, and the plugin does not depend on `libopen62541.dll`.

## Environment used

| Item | Path |
|---|---|
| qtopcua sources | `C:\g\qtopcua` (clean checkout of tag `v6.12.0`) |
| Qt kit | `C:\Qt\6.12.0\llvm-mingw_64` |
| Compiler | `C:\Qt\Tools\llvm-mingw2217_64` |
| CMake / Ninja | `C:\Qt\Tools\CMake_64`, `C:\Qt\Tools\Ninja` |

Adjust the paths for your machine. The sources must be unmodified (`git status` clean) to get the default backend.

## 1. Configure

```powershell
$env:PATH = "C:\Qt\Tools\llvm-mingw2217_64\bin;C:\Qt\6.12.0\llvm-mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
cd C:\g\qtopcua
cmake -S . -B build-6.12.0 -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/llvm-mingw_64 `
  -DCMAKE_INSTALL_PREFIX=C:/Qt/6.12.0/llvm-mingw_64 `
  -DCMAKE_C_COMPILER=C:/Qt/Tools/llvm-mingw2217_64/bin/clang.exe `
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/llvm-mingw2217_64/bin/clang++.exe `
  -DQT_BUILD_STANDALONE_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
```

Do **not** pass `-Dopen62541_DIR` or `-DFEATURE_system_open62541=ON`. Either one makes the plugin use an external open62541 library.

Check that the bundled backend was selected:

```powershell
Select-String build-6.12.0\CMakeCache.txt -Pattern 'QT_FEATURE_system_open62541|QT_FEATURE_open62541:'
```

Expected:

```
QT_FEATURE_open62541:INTERNAL=ON
QT_FEATURE_system_open62541:INTERNAL=OFF
```

`build-6.12.0\config.summary` should also show `Open62541 ... yes`.

## 2. Build

```powershell
cmake --build build-6.12.0 --parallel
```

The last step links `plugins\opcua\open62541_backend.dll`.

## 3. Install into the Qt kit

Close any running OPC UA apps first (the DLLs are locked while in use).

```powershell
cmake --install build-6.12.0
```

This overwrites `Qt6OpcUa.dll`, `plugins\opcua\open62541_backend.dll` and the CMake and QML files under the kit.
To keep the previous install, back up `bin\Qt6OpcUa.dll` and `plugins\opcua\open62541_backend.dll` before installing.

If the kit previously had a system-open62541 build, delete the old `C:\Qt\6.12.0\llvm-mingw_64\bin\libopen62541.dll`.
The new plugin does not import it, and a stale copy only causes confusion.

## 4. Verify the plugin

```powershell
& "C:\Qt\Tools\llvm-mingw2217_64\bin\llvm-objdump.exe" -p C:\Qt\6.12.0\llvm-mingw_64\plugins\opcua\open62541_backend.dll |
  Select-String 'DLL Name'
```

The imports must **not** include `libopen62541.dll`. Qt6OpcUa.dll, Qt6Core.dll, libc++.dll, WS2_32.dll and system DLLs are expected.

## 5. Build and run the app (`qtcon-ua-sub`)

```powershell
$env:PATH = "C:\Qt\Tools\llvm-mingw2217_64\bin;C:\Qt\6.12.0\llvm-mingw_64\bin;$env:PATH"
cd C:\g\QtUASamples\qtcon-ua-sub\build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug
mingw32-make
C:\g\QtUASamples\deploy.cmd "C:\g\QtUASamples\qtcon-ua-sub\build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug\qtcon-ua-sub.exe" "" "" 1
```

By default, the script creates a uniquely named folder under `%TEMP%` using the current date and time (`yyyyMMdd-HHmmss`), then prints its location and copies the path to the clipboard for opening in File Explorer. The executable and its deployed Qt files are placed there; run the deployed executable from that folder. You can provide a deploy directory as the third argument.

Re-run `deploy.cmd` after reinstalling qtopcua to create a fresh deployment. A stale plugin in the deployment folder can cause `Failed to load OPC UA plugin: "open62541"`.

The repository-root `deploy.cmd` requires a full path to the `.exe`. Pass `1` as the fourth argument for qtopcua apps so deployment fails if `opcua\open62541_backend.dll` is missing; for example: `deploy.cmd "C:\path\to\app.exe" "C:\Qt\6.12.0\llvm-mingw_64" "C:\path\to\deploy" 1`.

Expected output: the client connects, then prints `Monitoring enabled ... Status: QOpcUa::Good`,
followed by `Subscription update - Value: ...` roughly once per second.

To see protocol details (hide the event-loop spam):

```powershell
$env:QT_LOGGING_RULES = "qt.opcua*=true;qt.opcua.plugins.open62541.sdk.eventloop=false"
```

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Failed to load OPC UA plugin: "open62541"` | Stale deployed plugin in the app folder. Re-run `deploy.cmd`. |
| Plugin imports `libopen62541.dll` | Configured with a system open62541. Reconfigure in a fresh build dir without `open62541_DIR`. |
| `cmake --install` fails with access denied | An app still has the DLLs loaded. Stop it and retry. |
| `build\` folder in the sources targets another Qt (e.g. 6.7.3) | Stale. Use a separate build dir such as `build-6.12.0`. |
| No console output when launched with plain redirection | `qDebug` goes to stderr. Set `QT_FORCE_STDERR_LOGGING=1` and redirect both streams (`> log.txt 2>&1`). |
