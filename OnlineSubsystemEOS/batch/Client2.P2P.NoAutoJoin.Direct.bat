@echo off
REM Tutorials 4 + 7: Second-client P2P Direct launch with -NoAutoJoin.
REM
REM Default P2P client behavior: client A creates the lobby, client B's
REM HandleFindSessionsCompleted finds it and auto-joins. That races
REM invite-driven joins, so the session-invite tutorial would never get
REM a chance to drive the join.
REM
REM -NoAutoJoin gates the post-login chain (HandleLoginCompleted ->
REM GetSanctions -> FindSessions -> JoinSession) off in
REM HandleLoginCompleted: the client still logs in (so it has a NetId,
REM can be invited, and can run Exec commands) but stays out of any
REM lobby until the invite-accept path (TestAcceptLastInvite or the
REM EOS Social Overlay's Accept button) runs JoinSession explicitly.
REM
REM Pair with TestSendSessionInvite from the host (Client.P2P.Direct.bat).
REM
REM Direct launch (bootstrapper-bypass) - see Client.P2P.Direct.bat for
REM the kick-test rationale.

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -NoAutoJoin ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
