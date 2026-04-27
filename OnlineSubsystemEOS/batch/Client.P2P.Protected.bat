@echo off
REM Tutorial 9 (P2P mesh): Launch a packaged P2P client through the EAC bootstrapper.
REM
REM Same pipeline as the server-mode Client.Protected.bat (package via
REM BuildCookRun, sign via ProtectEOSPackage, launch via start_protected_game.exe),
REM but:
REM   - Package is built with P2PMODE=1.
REM   - -Artifact=P2PClient is passed to ProtectEOSPackage so Settings.json
REM     carries the P2PClient Dev Portal credentials.
REM   - -epicapp="P2PClient" below picks the matching OSS artifact entry at runtime.
REM
REM With both peers launched through this script: each one registers the other
REM as an EAC peer via the lobby-join delegate, the SDK handshake completes
REM (LocalAuthComplete -> RemoteAuthComplete), and the EOS P2P "EOSAntiCheat"
REM socket carries AC messages alongside (but separately from) NetDriverEOS.
REM
REM Copy this file to Client.P2P.Protected.local.bat, edit the path + DevAuth
REM credential for your machine, and run.

REM cd into the package root so UE's ../../../Engine/... relative DLL loads
REM resolve correctly when the game inherits cwd from the bootstrapper process.
cd /d "<Path to packaged output root (contains start_protected_game.exe)>"

start_protected_game.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
