@echo off
taskkill /f /im game.exe >nul 2>&1

echo Compiling with local Raylib and GLFW environment...

g++ src/main.cpp src/game_logic.cpp src/data_manager.cpp -o game.exe -I./include -L./lib -finput-charset=UTF-8 -fexec-charset=UTF-8 -lraylib -lglfw3 -lopengl32 -lgdi32 -lwinmm

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Compilation failed! Please check your include/lib folders.
    pause
    exit /b 1
)

echo.
echo =========================================
echo   Compilation successful! Starting...
echo =========================================
echo.

start "" game.exe