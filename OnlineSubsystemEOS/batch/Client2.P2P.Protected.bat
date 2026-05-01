@echo off
REM Tutorial 10 (P2P listen-server): Second packaged P2P client, launched through the EAC
REM bootstrapper. Same shape as Client.P2P.Protected.bat - use a different
REM DevAuthTool credential name so this client logs in as a distinct EOS user.
REM See that file's header for the full flow.

cd /d "<Path to packaged output root (contains start_protected_game.exe)>"

start_protected_game.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
