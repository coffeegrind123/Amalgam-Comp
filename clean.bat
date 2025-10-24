@echo off
REM Clean build artifacts and intermediate files

setlocal enabledelayedexpansion

echo ========================================
echo Amalgam Clean Script
echo ========================================
echo.

echo Cleaning Visual Studio artifacts...
echo.

REM Remove build output directories
if exist "build\" (
    echo Removing build\...
    rmdir /s /q "build\"
)

if exist "output\" (
    echo Removing output\...
    rmdir /s /q "output\"
)

if exist ".vs\" (
    echo Removing .vs\...
    rmdir /s /q ".vs\"
)

REM Clean project intermediate directories
if exist "Amalgam\x64\" (
    echo Removing Amalgam\x64\...
    rmdir /s /q "Amalgam\x64\"
)

if exist "AmalgamLoader\src\x64\" (
    echo Removing AmalgamLoader\src\x64\...
    rmdir /s /q "AmalgamLoader\src\x64\"
)

REM Remove solution user files
if exist "*.suo" del /q "*.suo"
if exist "*.user" del /q "*.user"

echo.
echo ========================================
echo Clean completed!
echo ========================================
echo.
echo You can now run build.bat to rebuild from scratch
echo.

pause
