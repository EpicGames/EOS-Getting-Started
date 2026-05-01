@echo off
REM Tutorial 10 (P2P listen-server): Launch a packaged P2P client DIRECTLY, bypassing
REM the EAC bootstrapper. Intentionally unprotected - useful for:
REM   - Confirming a protected peer kicks this one via OnPeerActionRequired
REM     (expected: kick after AuthenticationTimeout seconds, configurable via
REM     [EOSAntiCheat] PeerAuthenticationTimeoutSeconds in DefaultEngine.ini).
REM   - Iterating on gameplay without re-running ProtectEOSPackage.
REM
REM Without the bootstrapper, the EOS SDK logs
REM     LogEOSSDK: LogEOSAntiCheat: [AntiCheatClient] Anti-cheat client not available.
REM on this client, and IEOSAntiCheat::GetClient() returns null, so no peer
REM handshake ever starts. Every other peer's SDK fires OnPeerActionRequired
REM with reason=AuthenticationFailed after the timeout.
REM
REM Copy to Client.P2P.Direct.local.bat and edit paths + credentials.

cd /d "<Path to packaged Client Binaries\Win64 dir>"

EOS_OSS_Tutorial.exe ^
    ThirdPersonMap ^
    -debug -game ^
    -AUTH_TYPE="developer" ^
    -AUTH_LOGIN="localhost:8081" ^
    -AUTH_PASSWORD="<DevAuthTool Credential name>" ^
    -epicapp="P2PClient"
