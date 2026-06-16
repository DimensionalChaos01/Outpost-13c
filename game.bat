@echo off
setlocal
echo [Outpost 13-C] Compiling...
wsl --cd "%~dp0" g++ -std=c++17 src/*.cpp -o build/outpost13c -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf
if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed. See errors above.
    pause >nul
    exit /b 1
)
echo.
echo [Outpost 13-C] Launching...
wsl --cd "%~dp0" ./build/outpost13c
