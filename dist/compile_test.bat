@echo off
REM Compile the test logging program with C++23 support

echo Compiling test_logging.cpp with C++23 support...

REM Try with MSVC if available
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    cl /std:c++latest /EHsc test_logging.cpp /Fetest_logging.exe
    if %ERRORLEVEL% EQU 0 (
        echo Compilation successful!
        echo Running test_logging.exe...
        test_logging.exe
    ) else (
        echo MSVC compilation failed.
    )
) else (
    REM Try with g++ if available
    where g++ >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Using g++ compiler...
        g++ -std=c++23 test_logging.cpp -o test_logging.exe
        if %ERRORLEVEL% EQU 0 (
            echo Compilation successful!
            echo Running test_logging.exe...
            test_logging.exe
        ) else (
            echo g++ compilation failed.
        )
    ) else (
        echo No compatible C++ compiler found. Please install MSVC or MinGW.
    )
)

pause
