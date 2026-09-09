@echo off
setlocal
cd /d "%~dp0"
if not exist .venv\Scripts\python.exe uv venv .venv --python 3.12
if errorlevel 1 exit /b 1
uv pip install --python .venv\Scripts\python.exe -r training\requirements.txt --index-url https://download.pytorch.org/whl/cpu
exit /b %errorlevel%
