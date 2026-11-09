@echo off
rem Usage: deploy.cmd <full_path_to_exe> [qt_dir] [deploy_dir] [require_opcua]
rem   require_opcua: set to 1 to fail if the OPC UA backend plugin is not deployed
rem Deploys the executable and Qt libraries to deploy_dir, or a new temp folder.
setlocal

if "%~1"=="" goto Usage
if /I not "%~1"=="%~f1" goto InvalidPath
if /I not "%~x1"==".exe" goto InvalidExe
set "EXE=%~1"
set "QT_DIR=%~2"
set "DEPLOY_DIR=%~3"
set "REQUIRE_OPCUA=%~4"
if "%REQUIRE_OPCUA%"=="" set "REQUIRE_OPCUA=0"
if "%QT_DIR%"=="" set "QT_DIR=C:\Qt\6.12.0\llvm-mingw_64"

if not exist "%EXE%" (
    echo ERROR: %EXE% not found. Build the project first.
    exit /b 1
)
if exist "%EXE%\" (
    echo ERROR: Expected an executable file, got a directory: %EXE%
    exit /b 1
)

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo ERROR: %QT_DIR%\bin\windeployqt.exe not found.
    exit /b 1
)

if "%DEPLOY_DIR%"=="" goto CreateTempDeployDir
for %%F in ("%DEPLOY_DIR%") do set "DEPLOY_DIR=%%~fF"
if exist "%DEPLOY_DIR%" if not exist "%DEPLOY_DIR%\" goto InvalidDeployDir
if not exist "%DEPLOY_DIR%\" mkdir "%DEPLOY_DIR%" || goto CreateDeployDirFailed
goto DeployDirReady

:CreateTempDeployDir
for /f %%I in ('powershell.exe -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "DEPLOY_TIMESTAMP=%%I"
if not defined DEPLOY_TIMESTAMP goto CreateTimestampFailed
set "DEPLOY_DIR=%TEMP%\QtDeploy-%~n1-%DEPLOY_TIMESTAMP%-%RANDOM%"
if exist "%DEPLOY_DIR%\" goto CreateTempDeployDir
mkdir "%DEPLOY_DIR%" || goto CreateDeployDirFailed

:DeployDirReady
set "DEPLOY_EXE=%DEPLOY_DIR%\%~nx1"
if /I not "%EXE%"=="%DEPLOY_EXE%" (
    copy /y "%EXE%" "%DEPLOY_EXE%" >nul || goto CopyExeFailed
)

rem windeployqt needs Qt's bin on PATH to resolve dependencies
set "PATH=%QT_DIR%\bin;%PATH%"

"%QT_DIR%\bin\windeployqt.exe" --no-translations --dir "%DEPLOY_DIR%" "%DEPLOY_EXE%" || exit /b 1

if exist "%DEPLOY_DIR%\opcua\open62541_backend.dll" (
    rem Some qtopcua builds require the open62541 DLL alongside the application.
    if exist "%QT_DIR%\bin\libopen62541.dll" (
        copy /y "%QT_DIR%\bin\libopen62541.dll" "%DEPLOY_DIR%\" >nul
        if errorlevel 1 (
            if not exist "%DEPLOY_DIR%\libopen62541.dll" (
                echo ERROR: could not copy libopen62541.dll
                exit /b 1
            )
            echo WARNING: libopen62541.dll is in use, keeping existing copy. Close the app and rerun to update.
        )
    )
) else if "%REQUIRE_OPCUA%"=="1" (
    echo ERROR: opcua\open62541_backend.dll was not deployed to %DEPLOY_DIR%.
    exit /b 1
)

set "QT_DEPLOY_PATH=%DEPLOY_DIR%"
powershell.exe -NoProfile -Command "Set-Clipboard -Value $env:QT_DEPLOY_PATH"
if errorlevel 1 (
    echo ERROR: Deployment completed at %DEPLOY_DIR%, but the path could not be copied to the clipboard.
    exit /b 1
)
echo Deployed files are in: %DEPLOY_DIR%
echo Deployment path copied to clipboard. Paste it into File Explorer's address bar.
endlocal
exit /b 0

:Usage
echo Usage: deploy.cmd ^<full_path_to_exe^> [qt_dir] [deploy_dir] [require_opcua]
exit /b 1

:InvalidPath
echo ERROR: Provide the full path to the executable.
exit /b 1

:InvalidExe
echo ERROR: The target must be an .exe file.
exit /b 1

:InvalidDeployDir
echo ERROR: The deploy directory path is an existing file: %DEPLOY_DIR%
exit /b 1

:CreateDeployDirFailed
echo ERROR: Could not create deploy directory: %DEPLOY_DIR%
exit /b 1

:CreateTimestampFailed
echo ERROR: Could not generate a timestamp for the temporary deploy directory.
exit /b 1

:CopyExeFailed
echo ERROR: Could not copy %EXE% to %DEPLOY_EXE%.
exit /b 1
