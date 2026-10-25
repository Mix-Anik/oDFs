@echo off
rem Starts one oDFs server and restarts it when it exits (the Windows twin of start_server.sh).
rem   usage: start_server.bat NAME PORT lan or public
rem   e.g.   start_server.bat cpm 27960 lan
rem Layout (set BASE / HOME_ROOT before calling to change it):
rem   BASE\oDFe.ded.x64.exe, BASE\baseq3\, BASE\defrag\*.pk3   engine, Quake 3 data, DeFRaG media / maps / zzz-defrag_vm.pk3
rem   HOME_ROOT\NAME\defrag\   that server's own files: NAME.cfg + common.cfg, games.log, q3config_server.cfg
rem lan = unlisted, public = listed on the master servers (the engine's "dedicated" 1 / 2, 1 and 2 are accepted too). There is no default.
rem "dedicated" is write protected once the engine runs, so it can only be given on the command line (here), not in a cfg file.
if "%~3"=="" (
	echo usage: %~nx0 NAME PORT lan or public
	exit /b 1
)
if not defined BASE set "BASE=%~dp0."
if not defined HOME_ROOT set "HOME_ROOT=%~dp0homes"
set "DEDICATED=%~3"
set "DEDMODE="
if /i "%DEDICATED%"=="lan" set "DEDMODE=1"
if "%DEDICATED%"=="1" set "DEDMODE=1"
if /i "%DEDICATED%"=="public" set "DEDMODE=2"
if "%DEDICATED%"=="2" set "DEDMODE=2"
if not defined DEDMODE (
	echo third argument must be lan or public
	exit /b 1
)

cd /d "%BASE%"
if not exist "%HOME_ROOT%\%~1\defrag" mkdir "%HOME_ROOT%\%~1\defrag"
:loop
oDFe.ded.x64.exe +set fs_basepath "%BASE%" +set fs_homepath "%HOME_ROOT%\%~1" +set fs_game defrag +set net_port %~2 +set dedicated %DEDMODE% +exec %~1.cfg
echo Server %~1 exited (%errorlevel%), restarting in 5s... Ctrl+C to stop.
timeout /t 5 >nul
goto loop
