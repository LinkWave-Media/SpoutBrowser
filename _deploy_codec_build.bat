@echo off
REM ============================================================
REM  Deploy codec-enabled libcef.dll to SpoutBrowser
REM ============================================================
REM  Run this AFTER _build_cef_with_codecs.bat completes.
REM  It copies the codec-enabled binaries into Release 1.1.
REM ============================================================

set BUILD_DIR=C:\cef_build
set RELEASE_DIR=%~dp0Releases\Release 1.1

REM Find the binary_distrib folder
set DISTRIB_DIR=
for /d %%d in ("%BUILD_DIR%\chromium_git\chromium\src\cef\binary_distrib\cef_binary_*_windows64") do (
    set DISTRIB_DIR=%%d
)

if "%DISTRIB_DIR%"=="" (
    echo ERROR: Could not find binary_distrib folder in %BUILD_DIR%
    echo Make sure _build_cef_with_codecs.bat completed successfully.
    pause
    exit /b 1
)

echo.
echo Source:      %DISTRIB_DIR%\Release\
echo Destination: %RELEASE_DIR%\
echo.
echo This will overwrite libcef.dll and related files.
echo Press Ctrl+C to cancel, or any key to continue...
pause

REM Copy the codec-enabled Release files
echo Copying libcef.dll...
copy /y "%DISTRIB_DIR%\Release\libcef.dll" "%RELEASE_DIR%\libcef.dll"

echo Copying chrome_elf.dll...
copy /y "%DISTRIB_DIR%\Release\chrome_elf.dll" "%RELEASE_DIR%\chrome_elf.dll" 2>nul

echo Copying other Release binaries...
for %%f in ("%DISTRIB_DIR%\Release\*.dll") do (
    echo   %%~nxf
    copy /y "%%f" "%RELEASE_DIR%\%%~nxf" >nul
)
for %%f in ("%DISTRIB_DIR%\Release\*.bin") do (
    echo   %%~nxf
    copy /y "%%f" "%RELEASE_DIR%\%%~nxf" >nul
)

echo.
echo ============================================================
echo  Deploy complete!
echo ============================================================
echo.
echo  libcef.dll has been updated with codec support.
echo  Launch SpoutBrowser.exe to test H.264 video playback.
echo.
pause
