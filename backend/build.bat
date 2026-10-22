@echo off
cd /d "%~dp0"

set "GCC=C:\msys64\ucrt64\bin\g++.exe"

if not exist "%GCC%" (
    echo ERROR: MSYS2 UCRT64 compiler not found.
    pause
    exit /b 1
)

"%GCC%" -std=c++17 -Wall main.cpp -IC:\msys64\ucrt64\include\mariadb -LC:\msys64\ucrt64\lib -lmariadb -lws2_32 -o certificate_backend.exe

if errorlevel 1 (
    echo.
    echo BUILD FAILED. Read the compiler errors above.
    pause
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL!
echo Starting Certificate Backend...
certificate_backend.exe
pause