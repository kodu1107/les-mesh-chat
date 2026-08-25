@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Sync-LES-Chat-MeshGate.ps1" %*
set "LES_EXIT=%ERRORLEVEL%"
echo.
if not "%LES_EXIT%"=="0" echo MeshGate sync failed with exit code %LES_EXIT%.
pause
exit /b %LES_EXIT%
