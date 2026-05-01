@echo off
REM Tutorial 10: Third client instance - same as Client.Protected.bat, provided
REM as a distinct file so each learner can fill in a different AUTH_PASSWORD
REM per client without duplicating by hand. Useful for the 3-client server-
REM mode regression test (host + 2 protected clients against a packaged server).

cd /d "<Path to packaged output root (contains start_protected_game.exe)>"

start_protected_game.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name for Client 3>" ^
    -epicapp="Client"
