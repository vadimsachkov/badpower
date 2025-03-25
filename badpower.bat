@echo off
setlocal enabledelayedexpansion

REM ================================================
REM badpower.bat
REM Version: 7
REM 
REM Purpose:
REM   Automatically shuts down the server if:
REM   - The target router (IP + MAC) is unreachable via ARP
REM   - For more than X minutes (e.g. 120)
REM   - AND the server has been running for at least Y minutes (e.g. 20)
REM
REM   This prevents the server from draining the UPS in case of long power outage.
REM   Used with Windows Task Scheduler (e.g. every 1–2 minutes).
REM
REM   Logs are written monthly to C:\temp\badpower_YYYYMM.log
REM ================================================

REM ----- Configuration -----
set scriptVersion=7
REM IP and MAC address of the router to monitor (no UPS)
set targetIP=190.190.32.11
set targetMAC=90-4E-2B-CA-0A-53
REM Max minutes without ARP before shutdown is triggered
set maxAllowedMinutes=120
REM Min server uptime (in minutes) before shutdown is allowed
set minUptimeBeforeShutdown=20

REM ----- Setup log file -----
if not exist C:\temp mkdir C:\temp
for /f "tokens=2 delims==." %%i in ('wmic os get LocalDateTime /value') do set curDT=%%i
set year=%curDT:~0,4%
set month=%curDT:~4,2%
set logfile=C:\temp\badpower_%year%%month%.log

REM ----- Get current timestamp -----
set day=%curDT:~6,2%
set hour=%curDT:~8,2%
set min=%curDT:~10,2%
set sec=%curDT:~12,2%
set currentTimestamp=%year%-%month%-%day% %hour%:%min%:%sec%
set curDate=%year%-%month%-%day%

call :logMsg "[%currentTimestamp%] Script v%scriptVersion% started"

REM ----- ARP refresh -----
REM Вадим . отключил так как требовало повышение прав. и так будут удаляться записи
REM arp -d %targetIP%
ping -n 1 -w 1000 %targetIP% >nul

arp -a %targetIP% | findstr /I "%targetMAC%" >nul
set err=%ERRORLEVEL%

if "%err%"=="0" (
    echo %currentTimestamp% > C:\temp\last_success.txt
    call :logMsg "[%currentTimestamp%] ARP check successful. Timestamp updated."
    goto :EOF
) else (
    call :logMsg "[%currentTimestamp%] ARP check FAILED."
)

REM ----- Read last success timestamp -----
if exist C:\temp\last_success.txt (
    set /p lastTime=<C:\temp\last_success.txt
    set "lastTime=!lastTime:~0,19!"
)
if "!lastTime!"=="" (
    for /f "skip=1 tokens=1" %%i in ('wmic os get LastBootUpTime') do (
        if not "%%i"=="" (
            set bootTime=%%i
            goto :gotBootFallback
        )
    )
    :gotBootFallback
    set lastTime=%bootTime:~0,4%-%bootTime:~4,2%-%bootTime:~6,2% %bootTime:~8,2%:%bootTime:~10,2%:%bootTime:~12,2%
)

call :logMsg "[%currentTimestamp%] Last successful response (from file): !lastTime!"

REM ----- Calculate minutes since last success -----
set /a curHour=1%hour% - 100
set /a curMin=1%min% - 100
set /a curTotalMin = curHour * 60 + curMin

set "lastDate=!lastTime:~0,10!"
set "lastClock=!lastTime:~11,5!"

for /f "tokens=1,2 delims=:" %%a in ("!lastClock!") do (
    set /a lastHour=1%%a - 100
    set /a lastMin=1%%b - 100
)
set /a lastTotalMin = lastHour * 60 + lastMin

if not "%curDate%"=="!lastDate!" (
    set /a curTotalMin += 1440
)

set /a diffMinutes = curTotalMin - lastTotalMin
call :logMsg "[%currentTimestamp%] Minutes since last successful response: %diffMinutes%"

REM ----- Check if too much time without ARP -----
if %diffMinutes% GEQ %maxAllowedMinutes% (

    REM ----- Get boot time -----
    for /f "skip=1 tokens=1" %%i in ('wmic os get LastBootUpTime') do (
        if not "%%i"=="" (
            set bootRaw=%%i
            goto :gotBoot
        )
    )
    :gotBoot
    set bootDate=%bootRaw:~0,8%
    set bootTime=%bootRaw:~8,6%
    set bootHour=%bootTime:~0,2%
    set bootMin=%bootTime:~2,2%
    set /a bootH=1%bootHour% - 100
    set /a bootM=1%bootMin% - 100
    set /a bootTotalMin=!bootH!*60 + !bootM!

    REM ----- Get current time (same method as in test_uptime.bat) -----
    for /f "tokens=2 delims==." %%i in ('wmic os get LocalDateTime /value') do set curRaw=%%i

    set curDateRaw=%curRaw:~0,8%
    set curTimeRaw=%curRaw:~8,6%
    set curHourRaw=%curTimeRaw:~0,2%
    set curMinRaw=%curTimeRaw:~2,2%
    set /a curH=1%curHourRaw% - 100
    set /a curM=1%curMinRaw% - 100
    set /a curTotalMin2=!curH!*60 + !curM!

    if not "%curDateRaw%"=="%bootDate%" (
        set /a curTotalMin2+=1440
    )

    set /a uptimeMinutes=curTotalMin2 - bootTotalMin
    call :logMsg "[%currentTimestamp%] Server uptime: !uptimeMinutes! minutes"

    if !uptimeMinutes! GEQ %minUptimeBeforeShutdown% (
        call :logMsg "[%currentTimestamp%] No ARP response for >= %maxAllowedMinutes% minutes AND uptime >= %minUptimeBeforeShutdown%. Initiating shutdown..."
        REM shutdown /s /t 60 /c "No ARP response from %targetIP% for over %maxAllowedMinutes% minutes"
    ) else (
        call :logMsg "[%currentTimestamp%] No ARP response for >= %maxAllowedMinutes%, but uptime < %minUptimeBeforeShutdown%. Shutdown skipped."
    )
)

if %diffMinutes% LSS %maxAllowedMinutes% (
    call :logMsg "[%currentTimestamp%] Absence of ARP response is less than %maxAllowedMinutes% minutes. Shutdown not required."
)

goto :EOF

:logMsg
echo %*
echo %* >> "%logfile%"
goto :EOF
