@echo off
setlocal enabledelayedexpansion

set "scriptDir=%~dp0"
set "currentDir=%cd:\=%"

if "%scriptDir:~-1%"=="\" (
    set "scriptDir=%scriptDir:\=%"
)

if "%currentDir%"=="%scriptDir%" (
    exit /b 0
)

echo Deleting unused language resources.
for /d %%F in ("%cd%\*-*") do (
    rd /s /q "%%F"
)
