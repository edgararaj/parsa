@echo off
setlocal

:: setup colors
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set ESC=%%b
)

set "GenDir=%~dp0\gen"

if exist %GenDir% (
	rd /s/q %GenDir% 2> NUL
	if %errorlevel% neq 0 (
		echo %ESC%[91mFailed to delete gen directory!%ESC%[0m
		exit /b
	)
)

if not exist %GenDir% mkdir %GenDir%

build\parsa.exe
