@echo off
REM ============================================================
REM  Build CEF 137 (Chromium 137) with Proprietary Codecs
REM  (H.264 / AAC / MP3 support)
REM ============================================================
REM
REM  Requirements:
REM    - Windows 10/11 x64
REM    - Visual Studio 2022 or later with Desktop C++ workload
REM    - Windows 10/11 SDK (10.0.22621.0 or newer)
REM    - Python 3 (already installed)
REM    - Git (already installed)
REM    - ~150 GB free disk space
REM    - ~16 GB RAM minimum
REM
REM  This script will:
REM    1. Install Google's depot_tools
REM    2. Download automate-git.py from CEF repository
REM    3. Download Chromium + CEF source (~40 GB)
REM    4. Build CEF with proprietary codecs (~4-8 hours)
REM    5. Output the codec-enabled libcef.dll in binary_distrib
REM
REM  USAGE: Run this from a standard Command Prompt (cmd.exe).
REM         Do NOT use PowerShell (depot_tools requires cmd.exe).
REM ============================================================

REM --- Configuration ---
set BUILD_DIR=C:\cef_build
set CEF_BRANCH=7151
set GN_DEFINES=is_official_build=true proprietary_codecs=true ffmpeg_branding=Chrome
set GN_ARGUMENTS=--ide=vs2022 --sln=cef --filters=//cef/*

REM --- Step 1: Create build directory ---
echo.
echo ============================================================
echo  Step 1: Creating build directory: %BUILD_DIR%
echo ============================================================
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Step 2: Install depot_tools ---
echo.
echo ============================================================
echo  Step 2: Installing depot_tools
echo ============================================================
if not exist "%BUILD_DIR%\depot_tools" (
    cd /d "%BUILD_DIR%"
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
) else (
    echo depot_tools already exists, skipping...
)

REM Add depot_tools to PATH (must be at the front)
set PATH=%BUILD_DIR%\depot_tools;%PATH%

REM Tell depot_tools to use locally installed VS, not download one
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

REM --- Step 3: Download automate-git.py ---
echo.
echo ============================================================
echo  Step 3: Downloading automate-git.py
echo ============================================================
if not exist "%BUILD_DIR%\automate" mkdir "%BUILD_DIR%\automate"
if not exist "%BUILD_DIR%\automate\automate-git.py" (
    cd /d "%BUILD_DIR%\automate"
    echo Downloading from CEF repository...
    curl -L -o automate-git.py "https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py"
    if not exist automate-git.py (
        echo.
        echo ERROR: Could not download automate-git.py
        echo Please download it manually from:
        echo   https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py
        echo Place it in: %BUILD_DIR%\automate\automate-git.py
        pause
        exit /b 1
    )
) else (
    echo automate-git.py already exists, skipping...
)

REM --- Step 4: Run the build ---
echo.
echo ============================================================
echo  Step 4: Starting CEF build (this will take 4-8 hours)
echo ============================================================
echo.
echo  Branch:     %CEF_BRANCH% (Chromium 137)
echo  GN_DEFINES: %GN_DEFINES%
echo  Build dir:  %BUILD_DIR%
echo.
echo  The build will download ~40 GB of source code and compile.
echo  Make sure you have at least 150 GB of free disk space.
echo.
echo  Press Ctrl+C to cancel, or any key to continue...
pause

cd /d "%BUILD_DIR%"
python automate\automate-git.py ^
    --download-dir="%BUILD_DIR%\chromium_git" ^
    --branch=%CEF_BRANCH% ^
    --minimal-distrib ^
    --client-distrib ^
    --force-clean ^
    --x64-build

if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================================
    echo  BUILD FAILED! Check the output above for errors.
    echo ============================================================
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  BUILD COMPLETE!
echo ============================================================
echo.
echo  The codec-enabled CEF binaries are in:
echo    %BUILD_DIR%\chromium_git\chromium\src\cef\binary_distrib\
echo.
echo  To use with SpoutBrowser:
echo    1. Find the Release\ folder in binary_distrib
echo    2. Copy libcef.dll to your SpoutBrowser Release 1.1\ folder
echo    3. Also copy any updated .pak, .dat, and .bin files
echo.
pause
