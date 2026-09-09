@echo off
cd /d "%~dp0"
if not exist build\Release\Containment.exe call build.cmd
if not exist build\Release\Containment.exe exit /b 1
start "" "build\Release\Containment.exe"
