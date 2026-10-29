@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo   BiteCraft Online Food Ordering System - Build Script
echo   Compiling Pure C HTTP Web Server Backend
echo ============================================================
echo.

:: Auto-detect Winget-installed LLVM-MinGW if not already in PATH
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261006-ucrt-x86_64\bin" (
    set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261006-ucrt-x86_64\bin;!PATH!"
)

:: Check for GCC (MinGW / w64devkit / MSYS2)
where gcc >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [FOUND] GCC compiler detected.
    echo [BUILD] Running: gcc -O2 -Wall server.c -o server.exe -lws2_32
    gcc -O2 -Wall server.c -o server.exe -lws2_32
    goto check_result
)

:: Check for Clang
where clang >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [FOUND] Clang compiler detected.
    echo [BUILD] Running: clang -O2 -Wall server.c -o server.exe -lws2_32
    clang -O2 -Wall server.c -o server.exe -lws2_32
    goto check_result
)

:: Check for MSVC (cl.exe)
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [FOUND] Microsoft Visual C++ (cl.exe) detected.
    echo [BUILD] Running: cl /O2 /W3 server.c ws2_32.lib /Fe:server.exe
    cl /O2 /W3 server.c ws2_32.lib /Fe:server.exe
    goto check_result
)

echo [ERROR] No C compiler (gcc, clang, or cl) found in your system PATH.
echo.
echo Quick Fix:
echo   1. Install MinGW-w64 or LLVM:
echo        winget install BrechtSanders.WinLibs.POSIX.UCRT
echo      or:
echo        winget install LLVM.LLVM
echo   2. Reopen command prompt and rerun build.bat.
pause
exit /b 1

:check_result
if exist server.exe (
    echo.
    echo ============================================================
    echo [SUCCESS] server.exe built successfully!
    echo ============================================================
    echo.
    echo To start the server, execute:
    echo    server.exe
    echo.
    echo Then open in your browser:
    echo    http://localhost:8080
    echo.
    set /p run="Would you like to start the server now? (Y/N): "
    if /i "!run!"=="Y" (
        server.exe
    )
) else (
    echo [ERROR] Build failed. Please inspect compiler errors above.
    pause
    exit /b 1
)
