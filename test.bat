@echo off
setlocal

REM :: setup colors
REM for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  REM set ESC=%%b
REM )

REM set "GenDir=%~dp0\gen"

REM if exist %GenDir% (
	REM rd /s/q %GenDir% 2> NUL
	REM if %errorlevel% neq 0 (
		REM echo %ESC%[91mFailed to delete gen directory!%ESC%[0m
		REM exit /b
	REM )
REM )

REM if not exist %GenDir% mkdir %GenDir%

build\parsa.exe %*
