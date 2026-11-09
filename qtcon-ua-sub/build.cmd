@echo off
setlocal
rem Usage: build.cmd [clean]
rem Uses the same build tree/settings as Qt Creator (Desktop Qt 6.12.0 llvm-mingw 64-bit, Debug)
rem Output: build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug\qtcon-ua-sub.exe

if not defined QT_DIR set "QT_DIR=C:\Qt\6.12.0\llvm-mingw_64"
if not defined MINGW_DIR set "MINGW_DIR=C:\Qt\Tools\llvm-mingw2217_64"
set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;C:\Qt\Tools\CMake_64\bin;%PATH%"

set "BUILD_DIR=%~dp0build\Desktop_Qt_6_12_0_llvm_mingw_64_bit_Debug"
if /i "%~1"=="clean" if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

cmake -S "%~dp0." -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_CXX_COMPILER="%MINGW_DIR%/bin/clang++.exe" -DCMAKE_C_COMPILER="%MINGW_DIR%/bin/clang.exe" -DCMAKE_MAKE_PROGRAM="%MINGW_DIR%/bin/mingw32-make.exe" || exit /b 1
cmake --build "%BUILD_DIR%" || exit /b 1

echo.
echo Built: %BUILD_DIR%\qtcon-ua-sub.exe
echo To see output when redirected, set QT_ASSUME_STDERR_HAS_CONSOLE=1
