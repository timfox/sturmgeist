@echo off
REM Script to check for crash dumps and report their locations

echo Checking for crash dumps...
echo.

set DUMP_LOG=crash_dumps.txt
echo Crash dump search results > %DUMP_LOG%
echo Timestamp: %DATE% %TIME% >> %DUMP_LOG%
echo. >> %DUMP_LOG%

REM Check current directory for .dmp files
echo Checking current directory for .dmp files...
dir /b /s *.dmp 2>nul >> %DUMP_LOG%

REM Check Windows temp directory
echo Checking Windows temp directory...
dir /b /s "%TEMP%\*.dmp" 2>nul >> %DUMP_LOG%

REM Check Windows minidump directory
echo Checking Windows minidump directory...
dir /b /s "%LOCALAPPDATA%\CrashDumps\*.dmp" 2>nul >> %DUMP_LOG%

REM Check for any .mdmp files (Chromium-style crash dumps)
echo Checking for .mdmp files...
dir /b /s *.mdmp 2>nul >> %DUMP_LOG%

echo.
echo Search completed. Results saved to %DUMP_LOG%
echo.

REM Display the results
type %DUMP_LOG%

echo.
pause
