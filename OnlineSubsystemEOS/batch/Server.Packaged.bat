@echo off
REM Tutorial 10: Launch the packaged dedicated server.
REM
REM Required for end-to-end AntiCheat tests: the editor Server.bat path
REM (UnrealEditor.exe -server) has WITH_EDITOR=1 so the plugin's AntiCheat
REM code is compiled out. Use this packaged-server path when you want the
REM server to actually run BeginSession, RegisterClient, and issue kicks.
REM
REM Servers do NOT launch through the bootstrapper - the EAC server API is
REM trusted-process-only. The integrity tool / start_protected_game.exe are
REM not involved server-side.
REM
REM Prereqs:
REM   RunUAT.bat BuildCookRun -project=<uproject> -noP4 -platform=Win64
REM       -serverconfig=Development -server -noclient -cook -allmaps -build
REM       -stage -pak -archive -archivedirectory=<OutputDir>
REM
REM Copy this file to batch/local/Server.Packaged.bat and edit the path.

cd /d "<Path to packaged Server dir>"

REM Pass the map name explicitly - a packaged server with no map arg defaults
REM to /Engine/Maps/Entry (empty), which welcomes connecting clients into a
REM black-screen empty world.
REM
REM Clean shutdown: type "quit" in the server's console window instead of
REM Ctrl+C. Ctrl+C kills the process before the async EndSession/DestroySession
REM EOS callbacks complete (the session times out backend-side regardless).
EOS_OSS_TutorialServer.exe ^
    ThirdPersonMap ^
    -log ^
    -epicapp="Server"
