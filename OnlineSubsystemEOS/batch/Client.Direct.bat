@echo off
REM Tutorial 10: Launch a packaged client DIRECTLY, bypassing the EAC bootstrapper.
REM
REM Intentionally unprotected - useful for:
REM   - Confirming the server kicks unprotected clients (expected: kick after
REM     EAC heartbeat timeout, ~60s).
REM   - Iterating on gameplay when you don't want to re-run ProtectEOSPackage.
REM
REM Without the bootstrapper, the EOS SDK logs
REM     LogEOSSDK: LogEOSAntiCheat: [AntiCheatClient] Anti-cheat client not available.
REM on the client, and the plugin's GetClient() returns null so no client-side
REM AntiCheat session is established.
REM
REM Copy this file to <name>.Direct.local.bat and edit paths + credentials.

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="Client"
