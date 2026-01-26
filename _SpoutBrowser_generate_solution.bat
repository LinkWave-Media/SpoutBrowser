@echo off
setlocal

:: =================================================================
:: SpoutBrowser Solution Generator
:: 
:: This script automates the creation of a Visual Studio solution.
:: Instead of building CEF from scratch, it works on top of prebuilt 
:: CEF binaries, injecting SpoutBrowser logic into the sample application.
::
:: Workflow:
:: 1. Downloads CEF distribution (from cef-builds.spotifycdn.com) to /_cef_binary.
:: 2. Patches CEF sources/CMake scripts with SpoutBrowser code using Python.
:: 3. Runs CMake to generate the solution in /_cef_binary/cef_binary_*/build.
::    (Note: Spout2 dependency is fetched by CMake during this step).
:: =================================================================

:: Configuration

:: See available distibutions: https://cef-builds.spotifycdn.com/index.html
set CEF_DISTRIBUTION=cef_binary_142.0.17+g60aac24+chromium-142.0.7444.176_windows64

:: Spout tag (for CMake FetchContent_Declare)
:: See avalable tags: https://github.com/leadedge/Spout2/tags
set SPOUT_TAG=2.007.017

:: DEV: You may skip patch (e.g. to not lose local changes) or skip both (e.g. to "git init, add, commit" for easier diffing)
set SKIP_PATCH=0
set SKIP_GENERATE=0


:: =================================================================
:: Common vars

set "SCRIPT_DIR=%~dp0."
set "CEF_DOWNLOAD_DIR=%SCRIPT_DIR%\_cef_binary"
set "CEF_ROOT=%CEF_DOWNLOAD_DIR%\%CEF_DISTRIBUTION%"


:: =================================================================

:1_DOWNLOAD_CEF

    echo ===========================================================
    
    if exist "%CEF_ROOT%\" (
        echo 1. CEF distribution found: "%CEF_ROOT%"
        goto :2_PATCH_CEF
    )

    echo 1. Downloading CEF: "%CEF_ROOT%"

    :: Use CMake in scripting mode (-P) to execute the download script
    cmake -D "CEF_DOWNLOAD_DIR=%CEF_DOWNLOAD_DIR%" ^
          -D "CEF_DISTRIBUTION=%CEF_DISTRIBUTION%" ^
          -P "%SCRIPT_DIR%\_SpoutBrowser_DownloadCEF.cmake"

    if errorlevel 1 (
        echo ERROR: CEF download failed.
        goto :ERROR
    )
    
:: =================================================================

:2_PATCH_CEF

    if "%SKIP_PATCH%"=="1" (
        echo 2. Skipping CEF patching.
        goto :3_CMAKE_GENERATE
    )

    echo ===========================================================
    echo 2. Patch CEF

    python ^
        "%SCRIPT_DIR%\_SpoutBrowser_PatchCEF.py" ^
        "%SCRIPT_DIR%" ^
        "%CEF_ROOT%"
    
    if errorlevel 1 (
        echo ERROR: Patching failed.
        goto :ERROR
    )
    
:: =================================================================

:3_CMAKE_GENERATE

    if "%SKIP_GENERATE%"=="1" (
        echo 3. Skipping CMake generation.
        goto :DONE
    )

    echo ===========================================================
    echo 3. CMake generate: "%CEF_ROOT%"

    set "BUILD_DIR=%CEF_ROOT%\build"
    if not exist "%BUILD_DIR%\" mkdir "%BUILD_DIR%"

    :: -DUSE_SANDBOX=ON

    cmake ^
        -D "SPOUT_TAG=%SPOUT_TAG%" ^
        -G "Visual Studio 17" ^
        -A x64 ^
        -B "%BUILD_DIR%" ^
        -S "%CEF_ROOT%"

    if errorlevel 1 (
        echo ERROR: CMake generation failed.
        goto :ERROR
    )

:: =================================================================

:DONE

    echo ===========================================================
    echo Solution generated!
    exit /b 0

:ERROR

    echo ===========================================================
    echo Solution generation failed
    exit /b 1
