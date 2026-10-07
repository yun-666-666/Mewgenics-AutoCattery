@echo off
cd /d "%~dp0"
python tools\breeding_web.py
if errorlevel 1 (
  echo Install Python 3.10+ and run: python -m pip install -r tools\breeding_requirements.txt
  pause
)
