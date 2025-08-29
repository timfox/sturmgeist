@echo off
REM Launch Wolfenstein: Enemy Territory (etl.exe) with logging enabled.
REM This will create a log file named "etl.log" in the same directory as this script.

REM Ensure etl.exe is in the same directory as this script, or adjust the path below if needed.

setlocal
set EXE=etl.exe
set LOG=etl.log

REM Remove old log if it exists
if exist "%LOG%" del "%LOG%"

REM Start the game and force log output to etl.log in the current directory
REM The +set fs_homepath . ensures logs and configs are written to the current folder (portable mode)
start "" "%EXE%" +set fs_game legacy +set logfile 2 +set logfilename "%LOG%"

REM The log file will be created as ".\etl.log" in the same folder as this script.