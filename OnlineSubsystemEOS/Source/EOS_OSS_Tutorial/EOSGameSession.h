// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "EOSGameSession.generated.h"

/**
 * 
 */
UCLASS()
class EOS_OSS_TUTORIAL_API AEOSGameSession : public AGameSession
{
	GENERATED_BODY()
	
protected:
	// Class constructor. We won't use this in this tutorial. 
	AEOSGameSession();

	// Function called when play begins.
	virtual void BeginPlay();

	//Function called when play ends. We are using this to destroy session. 
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	// This function exists on the base class AGameSession. We want to override it so we don't try to login from a dedicated server. 
	virtual bool ProcessAutoLogin();

	// This function will be used to check if our session is full . It's better to check when a player joins rather than seeing if RegisterPlayer fails as it is async. 
	virtual FString ApproveLogin(const FString& Options);

	// Function called when players join dedicated server. Not used in this tutorial. 
	virtual void PostLogin(APlayerController* NewPlayer);

	// Function called when players leave the dedicated server. Trigger UnregisterPlayer from base class and used to End Sesion. 
	virtual void NotifyLogout(const APlayerController* ExitingPlayer);

	// Hardcoding the session name for this tutorial. 
	FName SessionName = "SessionName"; 

	// Hardcoding the max number of players in a session.
	const int MaxNumberOfPlayersInSession = 3;

	// Variable to keep track of the number of players in a session.
	int NumberOfPlayersInSession = 0;

	// Function to create an EOS session. 
	void CreateSession(FName KeyName = "KeyName", FString KeyValue= "KeyValue");

	// Callback function. This function will run when creating the session compeletes. 
	void HandleCreateSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for session creation. 
	FDelegateHandle CreateSessionDelegateHandle;

	// Used to keep track if the session exists or not. 
	bool bSessionExists = false;

	// Function to register our players in the EOS Session. 
	virtual void RegisterPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, bool bWasFromInvite=false);

	// Callback function. This function will run when registering the player compeletes. 
	void HandleRegisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate to bind callback event for register player. 
	FDelegateHandle RegisterPlayerDelegateHandle;

	// Function to unregister our players in the EOS Session.
	virtual void UnregisterPlayer(const APlayerController* ExitingPlayer);
	
	// Callback function. This function will run when unregistering the player compeletes.
	void HandleUnregisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate to bind callback event for unregister player. 
	FDelegateHandle UnregisterPlayerDelegateHandle;

	// Tutorial 3: Server-driven session attribute update. Real games change
	// session-level attributes in response to gameplay events (player join,
	// match start, round end, etc.) - never via client RPC, since clients
	// don't have authority over server-owned sessions. Demonstrated here:
	// Phase=Lobby at create time, flipped to Phase=InProgress on first
	// player join (HandleRegisterPlayerCompleted).
	void HandleUpdateSessionCompleted(FName SessionName, bool bWasSuccessful);
	FDelegateHandle UpdateSessionDelegateHandle;

	// Function to start EOS Session.
	void StartSession();

	// Callback function. This function will run when start session compeletes.
	void HandleStartSessionCompleted(FName SessionName, bool bWasSuccessful); 

	// Delegate to bind callback event for start session. 
	FDelegateHandle StartSessionDelegateHandle;

	// Function to end EOS Session. 
	void EndSession();

	// Callback function. This function will run when end session compeletes.
	void HandleEndSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for end session. 
	FDelegateHandle EndSessionDelegateHandle;

	// Function to Destroy EOS Session.
	void DestroySession();

	// Callback function. This function will run when destroy session completes.
	void HandleDestroySessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for destroy session.
	FDelegateHandle DestroySessionDelegateHandle;

#if !P2PMODE
protected:
	// EOS RTC room name for this match. Chosen by the server so every player joins the same room.
	FString VoiceRoomName;

	// Mints a per-player voice token via the EOS Voice Web API and forwards it to the owning
	// client through Client_ReceiveVoiceCredentials. Called from HandleRegisterPlayerCompleted.
	void RequestVoiceCredentialsForPlayer(class AEOSPlayerController* TargetPC, const FUniqueNetIdRef& PlayerId);

	// Finds the AEOSPlayerController whose PlayerState owns the supplied FUniqueNetId.
	class AEOSPlayerController* FindPlayerControllerByNetId(const FUniqueNetIdRef& PlayerId) const;

#if ACMODE
	// Tutorial 9: Bound to IEOSAntiCheat::OnViolation. Kicks via AGameSession::KickPlayer.
	// PlayerId null = PEER_SELF (P2P only); never reaches server-mode.
	void HandleAntiCheatViolation(const FUniqueNetIdPtr& PlayerId, const FString& Reason);

	// Tutorial 9: Bound to IEOSAntiCheatServer::OnMessageToClient. Forwards the opaque bytes
	// to the owning AEOSPlayerController via its Client_AntiCheatMessage reliable RPC.
	void HandleAntiCheatMessageToClient(const FUniqueNetIdRef& PlayerId, const TArray<uint8>& Bytes);

	FDelegateHandle AntiCheatViolationDelegateHandle;
	FDelegateHandle AntiCheatMessageToClientDelegateHandle;

public:
	// Tutorial 9: Called from AEOSPlayerController's Server_NotifyAntiCheatReady RPC once the
	// client has finished loading. Registers the player with the AntiCheat server at that point
	// (not before - see the IEOSAntiCheatServer::RegisterClient comment for the heartbeat
	// timeout that kicks in if we register while the client is still blocked).
	void RegisterAntiCheatClient(const FUniqueNetIdRef& PlayerId);
#endif
#endif
};
