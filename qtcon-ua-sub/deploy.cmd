@echo off
rem Usage: deploy.cmd [exe_path_or_build_dir] [qt_dir]
rem   exe_path_or_build_dir: full path to the .exe, or a directory containing qtcon-ua-sub.exe
rem Deploys the exe with Qt libs and the OPC UA open62541 plugin.
setlocal

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=%~dp0build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug"
set "QT_DIR=%~2"
if "%QT_DIR%"=="" set "QT_DIR=C:\Qt\6.12.0\llvm-mingw_64"

rem A directory contains "\" after appending it; a file path does not
if exist "%TARGET%\" (
    set "EXE=%TARGET%\qtcon-ua-sub.exe"
) else (
    set "EXE=%TARGET%"
)
for %%F in ("%EXE%") do set "BUILD_DIR=%%~dpF"
if "%BUILD_DIR:~-1%"=="\" set "BUILD_DIR=%BUILD_DIR:~0,-1%"

if not exist "%EXE%" (
    echo ERROR: %EXE% not found. Build the project first.
    exit /b 1
)
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo ERROR: %QT_DIR%\bin\windeployqt.exe not found.
    exit /b 1
)

rem windeployqt needs Qt's bin on PATH to resolve dependencies
set "PATH=%QT_DIR%\bin;%PATH%"

"%QT_DIR%\bin\windeployqt.exe" --no-translations "%EXE%" || exit /b 1

rem Only needed when the OPC UA plugin was built against a system open62541 (libopen62541.dll in Qt's bin);
rem the default qtopcua build bundles open62541 into the plugin.
if exist "%QT_DIR%\bin\libopen62541.dll" (
    copy /y "%QT_DIR%\bin\libopen62541.dll" "%BUILD_DIR%\" >nul
    if errorlevel 1 (
        if not exist "%BUILD_DIR%\libopen62541.dll" (
            echo ERROR: could not copy libopen62541.dll
            exit /b 1
        )
        echo WARNING: libopen62541.dll is in use, keeping existing copy. Close the app and rerun to update.
    )
)

if not exist "%BUILD_DIR%\opcua\open62541_backend.dll" (
    echo ERROR: opcua\open62541_backend.dll was not deployed.
    exit /b 1
)

echo Deployed to %BUILD_DIR%
endlocal
