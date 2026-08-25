@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-LES-Chat.ps1" %*
set "LES_EXIT=%ERRORLEVEL%"
echo.
if not "%LES_EXIT%"=="0" echo Installation failed with exit code %LES_EXIT%.
pause
exit /b %LES_EXIT%
