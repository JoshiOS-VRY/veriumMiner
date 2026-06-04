@echo off
REM Verium pool miner (Windows). Place next to cpuminer.exe from a release zip,
REM or run from the extracted archive (contrib\windows\mine-verium-pool.bat).
REM Edit VRM_ADDRESS and WORKER below, then double-click or run from cmd.

set VRM_ADDRESS=VYourAddress
set WORKER=worker1
set THREADS=0
set POOL=stratum+tcp://mine.vericonomy.com:3333

cd /d "%~dp0"
if exist "%~dp0..\..\cpuminer.exe" (
  cd /d "%~dp0..\.."
) else if exist "%~dp0cpuminer.exe" (
  cd /d "%~dp0"
) else (
  echo cpuminer.exe not found. Extract a release zip or copy this .bat beside cpuminer.exe.
  pause
  exit /b 1
)

cpuminer.exe -o %POOL% -u %VRM_ADDRESS%.%WORKER% -p x -t %THREADS%
pause
