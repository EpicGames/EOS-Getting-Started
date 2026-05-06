@echo off
REM Tutorials 4 + 5 + 10: Second-client Protected launch (via EAC bootstrapper)
REM with -NoAutoJoin.
REM
REM The default post-login chain (HandleLoginCompleted -> GetSanctions ->
REM FindSessions -> JoinSession) auto-joins any matching session as soon
REM as the client logs in. That races invite-driven joins, so the
REM session-invite tutorial would never get a chance to drive the join.
REM
REM -NoAutoJoin gates that chain off in HandleLoginCompleted: the client
REM still logs in (so it has a NetId, can be invited, and can run Exec
REM commands) but stays out of any session until the invite-accept path
REM (TestAcceptLastInvite or the EOS Social Overlay's Accept button)
REM runs JoinSession explicitly.
REM
REM Pair with TestSendSessionInvite from the host (Client.Protected.bat).
REM
REM Protected launch (via start_protected_game.exe) - required for AC kick
REM verification (Tutorial 10). Use Client2.NoAutoJoin.Direct.bat instead
REM when the bootstrapper-bypass kick test is what you want.

cd /d "<Path to packaged output root (contains start_protected_game.exe)>"

start_protected_game.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -NoAutoJoin ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name for Client 2>" ^
    -epicapp="Client"
