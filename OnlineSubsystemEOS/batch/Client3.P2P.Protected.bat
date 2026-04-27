@echo off
REM Tutorial 9 (P2P mesh): Third packaged P2P client, launched through the EAC
REM bootstrapper. Use a different DevAuthTool credential than Client.P2P.Protected.bat
REM and Client2.P2P.Protected.bat so this client logs in as a distinct EOS user.
REM See Client.P2P.Protected.bat's header for the full flow.
REM
REM Used in the 3-client kick-telemetry scenario: 1 host (this can be Client) + 1
REM other protected non-host + 1 unprotected (.Direct) non-host. The unprotected
REM client gets kicked by the host's SDK; the other protected non-host fires the
REM Server_PeerViolationDetected RPC so the host's log shows the non-host's
REM independent detection.

cd /d "<Path to packaged output root (contains start_protected_game.exe)>"

start_protected_game.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
