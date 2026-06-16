@echo off
REM Solo miner (Windows). Requires veriumd running with RPC enabled (verium.conf).
REM Edit RPC_USER, RPC_PASS, and COINBASE_ADDR below, then double-click or run from cmd.

set RPC_USER=your_rpc_user
set RPC_PASS=your_rpc_password
set COINBASE_ADDR=VYourPayoutAddress
set RPC_URL=http://127.0.0.1:33987
set THREADS=0

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

cpuminer.exe -o %RPC_URL% -O %RPC_USER%:%RPC_PASS% --coinbase-addr=%COINBASE_ADDR% ^
  --no-getwork --no-stratum --no-longpoll -t %THREADS% --profile dedicated
pause
