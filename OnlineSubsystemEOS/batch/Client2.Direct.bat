@echo off
REM Tutorial 10: Direct (bootstrapper-bypass) launch for the second client.
REM See Client.Direct.bat for the kick-test rationale.

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name for Client 2>" ^
    -epicapp="Client"
