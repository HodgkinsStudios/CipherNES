@echo off
rem Created by Jacob Hodgkins
setlocal
set "BIN=%~dp0build\windows\bin"
if not exist "%BIN%\CipherNES.exe" (
  echo CipherNES.exe was not found. Run build_windows.bat first.
  exit /b 1
)
pushd "%BIN%"
CipherNES.exe %*
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
