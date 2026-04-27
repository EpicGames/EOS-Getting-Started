@echo off
REM Tutorial 9 (P2P mesh): Second packaged P2P client, launched DIRECTLY
REM (unprotected). Use a different DevAuthTool credential than Client.P2P.Direct.bat.
REM See that file's header for the kick behavior you should expect.

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
