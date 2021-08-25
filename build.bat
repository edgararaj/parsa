@echo off
:: setup colors
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set ESC=%%b
)

set RootDir=%~dp0
set SrcDir=%RootDir%src
set BuildDir=%RootDir%build

set CommonCompilerFlags=-nologo -Od -GR- -Gm- -EHa- -Zi -Oi -WX -W4 -wd4100 -wd4201 -wd4189 -wd4701 -std:c++latest -DPARSA_DEBUG
set CommonLinkerFlags=-opt:ref -incremental:no -subsystem:console -nodefaultlib kernel32.lib libucrt.lib libvcruntime.lib libcmt.lib

if exist "%BuildDir%" (
	rd /s/q "%BuildDir%" 2> NUL
	if %errorlevel% neq 0 (
		echo %ESC%[91mFailed to delete build directory!%ESC%[0m
		exit /b
	)
)

if not exist %BuildDir% (
	mkdir %BuildDir%
	pushd %BuildDir%

	cl %CommonCompilerFlags% %RootDir%\main.cpp -Fmparsa -Feparsa -link %CommonLinkerFlags% advapi32.lib
	call :CheckCompile

	popd
)

exit /b

:CheckCompile
if %errorlevel% neq 0 (
	echo %ESC%[91mFailed to compile! ;(%ESC%[0m
) else (
	echo %ESC%[92mCompiled sucessfully!%ESC%[0m
)
