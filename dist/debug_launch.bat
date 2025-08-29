@echo off
REM Debug Launch Script for Wolfenstein: Enemy Territory (etl.exe)
REM This will create detailed logs to help diagnose startup crashes

setlocal
set EXE=etl.exe
set LOG=etl_debug.log

REM Remove old log if it exists
if exist "%LOG%" del "%LOG%"

REM Start the game with enhanced debug options
REM The +set fs_homepath . ensures logs and configs are written to the current folder (portable mode)
start "" "%EXE%" +set fs_game legacy +set logfile 3 +set developer 1 +set com_logfile 2 +set fs_homepath "." +set fs_debug 1 +set logfilename "%LOG%"

echo Debug launch initiated. Check for %LOG% after the game exits.
