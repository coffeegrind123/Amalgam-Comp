@echo off
REM Build script that mimics GitHub Actions workflow
REM This uses the same msbuild command as CI to ensure consistent builds

setlocal enabledelayedexpansion

echo ========================================
echo Amalgam Build Script
echo ========================================
echo.

REM Check if msbuild is already in PATH
where msbuild >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo MSBuild not found in PATH, searching for Visual Studio...
    echo.

    REM Try to find Visual Studio in common locations
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

    if exist "!VSWHERE!" (
        echo Using vswhere to locate Visual Studio...
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find Common7\Tools\VsDevCmd.bat`) do (
            set "VSDEVCMD=%%i"
        )
    )

    REM If vswhere didn't work, try common paths manually
    if not defined VSDEVCMD (
        echo Searching common Visual Studio paths...

        REM VS 2022 Build Tools
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
        )
        REM VS 2022 Community
        if not defined VSDEVCMD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
        )
        REM VS 2022 Professional
        if not defined VSDEVCMD if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
        )
        REM VS 2022 Enterprise
        if not defined VSDEVCMD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
        )
        REM VS 2019 Build Tools
        if not defined VSDEVCMD if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
        )
        REM VS 2019 Community
        if not defined VSDEVCMD if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" (
            set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
        )
    )

    if defined VSDEVCMD (
        echo Found Visual Studio at: !VSDEVCMD!
        echo.
        echo Initializing Visual Studio environment...
        call "!VSDEVCMD!" -arch=x64 -host_arch=x64
        if !ERRORLEVEL! NEQ 0 (
            echo ERROR: Failed to initialize Visual Studio environment
            pause
            exit /b 1
        )
        echo.
    ) else (
        echo ERROR: Could not find Visual Studio installation
        echo.
        echo Please install Visual Studio 2019/2022 with C++ Build Tools
        echo Or manually set the path to VsDevCmd.bat
        pause
        exit /b 1
    )
) else (
    echo MSBuild found in PATH
    echo.
)

REM Configuration and Platform from GitHub Actions
set CONFIGURATION=ReleaseFreetypeAVX2
set PLATFORM=x64

echo Configuration: %CONFIGURATION%
echo Platform: %PLATFORM%
echo.

REM Restore NuGet packages
echo Restoring NuGet packages...
nuget restore Amalgam.sln
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to restore NuGet packages
    echo Continuing anyway...
)
echo.

REM Build using the exact same command as GitHub Actions
echo Building Amalgam...
echo Command: msbuild Amalgam.sln /p:Platform=%PLATFORM% /p:Configuration=%CONFIGURATION%
echo.

msbuild Amalgam.sln /p:Platform=%PLATFORM% /p:Configuration=%CONFIGURATION%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build succeeded!
    echo ========================================
    echo.
    echo Output location:
    echo   output\%PLATFORM%\Release\
    echo.
) else (
    echo.
    echo ========================================
    echo Build FAILED with error code: %ERRORLEVEL%
    echo ========================================
    echo.
)

pause
exit /b %ERRORLEVEL%
