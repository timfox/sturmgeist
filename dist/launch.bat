@echo off
REM Enhanced debugging launch script for Wolfenstein: Enemy Territory (etl.exe)
REM This will create a log file and provide more debugging information

REM Ensure etl.exe is in the same directory as this script, or adjust the path below if needed.

setlocal
set EXE=etl.exe
set LOG=etl.log
set DEBUG_LOG=etl_debug.txt

echo Starting enhanced debug logging for %EXE% > %DEBUG_LOG%
echo Current directory: %CD% >> %DEBUG_LOG%
echo Timestamp: %DATE% %TIME% >> %DEBUG_LOG%

REM Check if the executable exists
if not exist "%EXE%" (
    echo ERROR: %EXE% not found in current directory! >> %DEBUG_LOG%
    echo ERROR: %EXE% not found in current directory!
    pause
    exit /b 1
)

echo Executable found: %CD%\%EXE% >> %DEBUG_LOG%

REM Remove old log if it exists
if exist "%LOG%" (
    echo Removing old log file: %LOG% >> %DEBUG_LOG%
    del "%LOG%"
)

REM List DLLs in current directory to check dependencies
echo. >> %DEBUG_LOG%
echo Listing DLLs in current directory: >> %DEBUG_LOG%
dir /b *.dll >> %DEBUG_LOG% 2>&1

REM Start the game with enhanced logging options
echo. >> %DEBUG_LOG%
echo Starting game with command: >> %DEBUG_LOG%
echo "%EXE%" +set fs_game legacy +set fs_homepath "." +set logfile 3 +set developer 1 +set com_logfile 2 +set logfilename "%LOG%" >> %DEBUG_LOG%

REM Launch the game with all debug parameters
start "" "%EXE%" +set fs_game legacy +set fs_homepath "." +set logfile 3 +set developer 1 +set com_logfile 2 +set logfilename "%LOG%"

echo Game launched at %TIME% >> %DEBUG_LOG%
echo Check for %LOG% after the game exits. >> %DEBUG_LOG%
echo If no log file appears, check %DEBUG_LOG% for diagnostic information. >> %DEBUG_LOG%

echo Game launched with enhanced debugging.
echo If the game crashes, check %DEBUG_LOG% for diagnostic information.