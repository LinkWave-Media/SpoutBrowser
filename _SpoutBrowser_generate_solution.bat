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
set CEF_DISTRIBUTION=cef_binary_137.0.19+g8a1c4ce+chromium-137.0.7151.121_windows64

:: Spout tag (for CMake FetchContent_Declare)
:: See avalable tags: https://github.com/leadedge/Spout2/tags
set SPOUT_TAG=2.007.017

:: DEV: You may skip patch (e.g. to not lose local changes) or skip both (e.g. to "git init, add, commit" for easier diffing)
set SKIP_PATCH=0
set SKIP_GENERATE=0

:: Detect Visual Studio version
set VS_GENERATOR=Visual Studio 17 2022
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %VSWHERE% (
    for /f "tokens=1 delims=." %%i in ('%VSWHERE% -latest -property installationVersion') do (
        if "%%i"=="15" set VS_GENERATOR=Visual Studio 15 2017
        if "%%i"=="16" set VS_GENERATOR=Visual Studio 16 2019
        if "%%i"=="17" set VS_GENERATOR=Visual Studio 17 2022
        if "%%i"=="18" set VS_GENERATOR=Visual Studio 18 2026
    )
)


:: =================================================================
:: Common vars

set "SCRIPT_DIR_RAW=%~dp0"
:: Remove trailing backslash if present
if "%SCRIPT_DIR_RAW:~-1%"=="\" set "SCRIPT_DIR_RAW=%SCRIPT_DIR_RAW:~0,-1%"

:: Find an available drive letter to subst to bypass MAX_PATH limit (260 characters)
set "VS_DRIVE="
for %%d in (S X Y Z B) do (
    if not exist %%d:\ (
        set "VS_DRIVE=%%d:\"
        goto :FOUND_DRIVE
    )
)
:FOUND_DRIVE

if not defined VS_DRIVE (
    echo WARNING: No available drive letter for subst. Path length limits may apply.
    set "SCRIPT_DIR=%SCRIPT_DIR_RAW%"
) else (
    echo Mapping "%SCRIPT_DIR_RAW%" to virtual drive %VS_DRIVE:~0,2%
    subst %VS_DRIVE:~0,2% "%SCRIPT_DIR_RAW%"
    set "SCRIPT_DIR=%VS_DRIVE%"
)

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
        "%SCRIPT_DIR%." ^
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
    if exist "%BUILD_DIR%\" rd /s /q "%BUILD_DIR%"
    mkdir "%BUILD_DIR%"

    :: -DUSE_SANDBOX=ON

    cmake ^
        -D "SPOUT_TAG=%SPOUT_TAG%" ^
        -D "USE_SANDBOX=OFF" ^
        -G "%VS_GENERATOR%" ^
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
    if defined VS_DRIVE (
        echo Unmapping virtual drive %VS_DRIVE:~0,2%
        subst %VS_DRIVE:~0,2% /d
    )
    exit /b 0

:ERROR

    echo ===========================================================
    echo Solution generation failed
    if defined VS_DRIVE (
        echo Unmapping virtual drive %VS_DRIVE:~0,2%
        subst %VS_DRIVE:~0,2% /d
    )
    exit /b 1
