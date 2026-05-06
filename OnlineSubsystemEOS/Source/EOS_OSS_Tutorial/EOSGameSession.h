// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "EOSGameSession.generated.h"

/**
 * Child class of AGameSession to hold dedicated-server session lifecycle.
 *
 * Only active when P2PMODE=0 - the GameMode only assigns this as
 * GameSessionClass under #if !P2PMODE (see EOS_OSS_TutorialGameMode.cpp);
 * the P2P listen-server path uses EOS Lobby instead.
 *
 * UHT note: we can't wrap the whole UCLASS in #if !P2PMODE - UHT requires
 * the UCLASS declaration to be visible unconditionally. The cpp gets
 * around this by wrapping every method's *body* in #if !P2PMODE, with a
 * tiny #else block providing stub bodies for the 9 functions that UHT
 * references at link time (the constructor + the 8 AGameSession virtual
 * overrides - the gen.cpp's vtable population and InternalConstructor
 * template need them). Everything else (CreateSession, RegisterPlayer,
 * Voice/AC helpers, etc.) only compiles when P2PMODE=0.
 */
UCLASS()
class EOS_OSS_TUTORIAL_API AEOSGameSession : public AGameSession
{
	GENERATED_BODY()

protected:
	/* =============== Tutorial 5 - Sessions ============================= */

	// Class constructor. We won't use this in this tutorial.
	AEOSGameSession();

	// Function called when play begins.
	virtual void BeginPlay();

	// Function called when play ends. Used to destroy the session.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	// Override base function so we don't try to login from a dedicated server.
	virtual bool ProcessAutoLogin();

	// Function to check if our session is full. It's better to check when a player joins rather than seeing if RegisterPlayer fails as it is async.
	virtual FString ApproveLogin(const FString& Options);

	// Function called when players join the dedicated server. Not used in this tutorial.
	virtual void PostLogin(APlayerController* NewPlayer);

	// Function called when players leave the dedicated server. Triggers UnregisterPlayer from base class and used to End Session.
	virtual void NotifyLogout(const APlayerController* ExitingPlayer);

	// Hardcoded name for the session.
	FName SessionName = "SessionName";

	// Hardcoded max number of players in a session.
	const int MaxNumberOfPlayersInSession = 3;

	// Variable to keep track of the number of players in the session.
	int NumberOfPlayersInSession = 0;

	// Used to keep track of whether the session exists or not.
	bool bSessionExists = false;

	// Function to create an EOS session.
	void CreateSession(FName KeyName = "KeyName", FString KeyValue = "KeyValue");

	// Callback function. Ran when CreateSession completes.
	void HandleCreateSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for CreateSession.
	FDelegateHandle CreateSessionDelegateHandle;

	// Function to register our players in the EOS Session.
	virtual void RegisterPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, bool bWasFromInvite=false);

	// Callback function. Ran when RegisterPlayer completes.
	void HandleRegisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate handle for RegisterPlayer.
	FDelegateHandle RegisterPlayerDelegateHandle;

	// Function to unregister our players in the EOS Session.
	virtual void UnregisterPlayer(const APlayerController* ExitingPlayer);

	// Callback function. Ran when UnregisterPlayer completes.
	void HandleUnregisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate handle for UnregisterPlayer.
	FDelegateHandle UnregisterPlayerDelegateHandle;

	// Callback function. Ran when UpdateSession completes. Server-driven session attribute update: real games change session-level attributes in response to gameplay events (player join, match start, round end, etc.) - never via client RPC, since clients don't have authority over server-owned sessions. Demonstrated here: Phase=Lobby at create time, flipped to Phase=InProgress on first player join (HandleRegisterPlayerCompleted).
	void HandleUpdateSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for UpdateSession.
	FDelegateHandle UpdateSessionDelegateHandle;

	// Function to start the EOS Session.
	void StartSession();

	// Callback function. Ran when StartSession completes.
	void HandleStartSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for StartSession.
	FDelegateHandle StartSessionDelegateHandle;

	// Function to end the EOS Session.
	void EndSession();

	// Callback function. Ran when EndSession completes.
	void HandleEndSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for EndSession.
	FDelegateHandle EndSessionDelegateHandle;

	// Function to destroy the EOS Session.
	void DestroySession();

	// Callback function. Ran when DestroySession completes.
	void HandleDestroySessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for DestroySession.
	FDelegateHandle DestroySessionDelegateHandle;

#if !P2PMODE
	/* =============== Tutorial 11 - Voice on server (P2PMODE=0 only) ============================= */

	// EOS RTC room name for this match. Chosen by the server so every player joins the same room.
	FString VoiceRoomName;

	// Function to mint a per-player voice token via the EOS Voice Web API and forward it to the owning client through Client_ReceiveVoiceCredentials. Called from HandleRegisterPlayerCompleted.
	void RequestVoiceCredentialsForPlayer(class AEOSPlayerController* TargetPC, const FUniqueNetIdRef& PlayerId);

	// Function to find the AEOSPlayerController whose PlayerState owns the supplied FUniqueNetId.
	class AEOSPlayerController* FindPlayerControllerByNetId(const FUniqueNetIdRef& PlayerId) const;

#if ACMODE
	/* =============== Tutorial 10 - Anti-cheat server (P2PMODE=0 + ACMODE=1) ============================= */

public:
	// Function to register a player with the AntiCheat server. Called from AEOSPlayerController's Server_NotifyAntiCheatReady RPC once the client has finished loading - not before, see the IEOSAntiCheatServer::RegisterClient comment for the heartbeat timeout that kicks in if we register while the client is still blocked.
	void RegisterAntiCheatClient(const FUniqueNetIdRef& PlayerId);

protected:
	// Multicast callback. Bound to IEOSAntiCheat::OnViolation. Kicks via AGameSession::KickPlayer. PlayerId null = PEER_SELF (P2P only); never reaches server-mode.
	void HandleAntiCheatViolation(const FUniqueNetIdPtr& PlayerId, const FString& Reason);

	// Delegate handle for the AC violation notification.
	FDelegateHandle AntiCheatViolationDelegateHandle;

	// Multicast callback. Bound to IEOSAntiCheatServer::OnMessageToClient. Forwards the opaque bytes to the owning AEOSPlayerController via its Client_AntiCheatMessage reliable RPC.
	void HandleAntiCheatMessageToClient(const FUniqueNetIdRef& PlayerId, const TArray<uint8>& Bytes);

	// Delegate handle for the AC message-to-client notification.
	FDelegateHandle AntiCheatMessageToClientDelegateHandle;
#endif // ACMODE
#endif // !P2PMODE
};
