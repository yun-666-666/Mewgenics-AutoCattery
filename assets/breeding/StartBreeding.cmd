@echo off
setlocal
cd /d "%~dp0"
if not exist "runtime\python.exe" (
  echo Missing bundled runtime. Extract the complete AutoCatteryWorkbench folder.
  pause
  exit /b 1
)
"runtime\python.exe" -B -X utf8 "tools\breeding_web.py"
if errorlevel 1 pause
