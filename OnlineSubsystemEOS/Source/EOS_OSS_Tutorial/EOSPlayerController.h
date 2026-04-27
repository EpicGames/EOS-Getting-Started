// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h" //Don't like declaring this here but getting weird compiler error with EOnJoinSessionCompleteResult
#include "OnlineSessionSettings.h"             // FOnlineSessionSearchResult - used by value in TOptional member below.
#include "EOSPlayerController.generated.h"

/** New log category for this tutorial. Bump verbosity with `log LogEOSOSSTutorial Verbose` (or VeryVerbose) to see success-path messages. */
DECLARE_LOG_CATEGORY_EXTERN(LogEOSOSSTutorial, Log, All);

#if !P2PMODE
// Forward declarations for the IVoiceChat API - kept out of the header proper so including this
// controller header doesn't pull the voice plugin into every TU that touches AEOSPlayerController.
class IVoiceChat;
class IVoiceChatUser;
struct FVoiceChatResult;
#endif


/**
 * Child class of APlayerController to hold EOS OSS code. 
 */

 //Need to forward declare classes used
class FOnlineSessionSearch;

UCLASS()
class EOS_OSS_TUTORIAL_API AEOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Class constructor. We won't use this in this tutorial. 
	AEOSPlayerController();

	// Load the position of the character from Player Data Storage - Should use USaveGame.
	void LoadGame(const TArray<uint8>& LoadData);

	// Save the position of the character to Player Data Storage - Should use USaveGame.
	void SaveGame();

protected:
	// Function called when play begins.
	virtual void BeginPlay();

	// Function called when play ends. Used to clear all async-OSS delegate handles so multicast lists
	// in the OSS interfaces don't retain stale bindings after the controller is destroyed.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Function to sign into EOS Game Services.
	void Login();

	// Callback function. This function is ran when signing into EOS Game Services completes. 
	void HandleLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	// Delegate to bind callback event for login. 
	FDelegateHandle LoginDelegateHandle;

	// Function to find EOS sessions. Hardcoded attribute key/value pair to keep things simple.
	void FindSessions(FName SearchKey = "KeyName", FString SearchValue = "KeyValue");

	// Callback function. This function will run when the session is found.
	void HandleFindSessionsCompleted(bool bWasSuccessful, TSharedRef<FOnlineSessionSearch> Search);

	// Delegate to bind callback event for when sessions are found.
	FDelegateHandle FindSessionsDelegateHandle;

	// This is the connection string for the client to connect to the dedicated server.
	FString ConnectString;

	// Stores a *copy* of the chosen search result from the find-sessions callback. It must be a value
	// (not a pointer into the search-result array): the TSharedRef<FOnlineSessionSearch> that owns
	// that array is released at the end of HandleFindSessionsCompleted, after which any pointer into
	// its SearchResults would dangle.
	TOptional<FOnlineSessionSearchResult> SessionToJoin;

	// Function to join the EOS session. 
	void JoinSession();

	// Callback function. This function will run when the session is joined. 
	void HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	// Delegate to bind callback event for join session.
	FDelegateHandle JoinSessionDelegateHandle;

	// Function used to enumerate title file storage files and read the 1st file in the returned list of files. 
	void LoadTitleData();

	// Callback function. This function is ran when retrieving all the files to be enumerated from the EOS backend completes. 
	void HandleEnumTitleFilesCompleted(bool bWasSuccessfull, const FString& Error);

	// Delegate to bind callback event for title file storage file enumeration.
	FDelegateHandle EnumTitleFilesDelegateHandle;

	// Callback function. This function is ran when a file has been read and the contents can be retrieved.  
	void HandleReadTitleFileCompleted(bool bWasSuccessfull, const FString& FileName); 

	// Delegate to bind callback event for title file storga file read.
	FDelegateHandle ReadTitleFileDelegateHandle;

	// Function to write to player data storage - called when player quits and game is saved. FileData is
	// passed by value because IOnlineUserCloud::WriteUserFile takes it by non-const reference (the OSS
	// may mutate the buffer during optional compression); giving it our own copy keeps the API clean.
	void WritePlayerDataStorage(FString FileName, TArray<uint8> FileData);
	
	// Callback function. This function is ran when writing to player data storage completes.
	void HandleWritePlayerDataStorageCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);

	// Delegate to bind callback event for title file storage enumeration.
	FDelegateHandle WritePlayerDataStorageDelegateHandle;

	// Function used to enumerate title file storage files and read the 1st file in the returned list of files. 
	void LoadPlayerData();

	// Callback function. This function is ran when retrieving all the files to be enumerated from the EOS backend completes. 
	void HandleEnumPlayerFilesCompleted(bool bWasSuccessfull, const FUniqueNetId& NetId);

	// Delegate to bind callback event for player file storage enumeration.
	FDelegateHandle EnumPlayerFilesDelegateHandle;

	// Callback function. This function is ran when a player data storage file has been read and the contents can be retrieved.  
	void HandleReadPlayerFileCompleted(bool bWasSuccessfull, const FUniqueNetId& UserId, const FString& FileName);

	// Delegate to bind callback event for player data file storage file read.
	FDelegateHandle ReadPlayerDataFileDelegateHandle;

public:
	// RPCs are declared outside the P2PMODE guard because UHT can't see preprocessor
	// blocks; the _Implementation bodies are #if-guarded in the .cpp instead.

	// Server -> client RPC: per-player EOS RTC room name + participant token + media-server URL.
	UFUNCTION(Client, Reliable)
	void Client_ReceiveVoiceCredentials(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl);

	// Tutorial 9: client signals load-complete + sends its EOS Connect IdToken for VerifyIdToken.
	UFUNCTION(Server, Reliable)
	void Server_NotifyAntiCheatReady(const FString& IdTokenJwt);

	// Tutorial 9: bidirectional opaque EOS AntiCheat byte relay.
	UFUNCTION(Server, Reliable)
	void Server_AntiCheatMessage(const TArray<uint8>& Bytes);

	UFUNCTION(Client, Reliable)
	void Client_AntiCheatMessage(const TArray<uint8>& Bytes);

	// Tutorial 9 (P2P branch): non-host's violation report - host logs for telemetry only.
	UFUNCTION(Server, Reliable)
	void Server_PeerViolationDetected(const FUniqueNetIdRepl& OffendingPlayer, const FString& Reason);

#if !P2PMODE
protected:
	// Per-session voice-chat opt-in. Checked when the server-issued credentials RPC arrives.
	bool bVoiceChatEnabled = true;

	// Teardown for the voice session. Chains LeaveChannel -> Logout -> ReleaseUser -> Disconnect. Called from EndPlay.
	void LeaveVoiceChat();

	// Per-step callbacks for the IVoiceChat Connect -> Login -> JoinChannel sequence.
	void HandleVoiceChatConnectComplete(const FVoiceChatResult& Result);
	void HandleVoiceChatLoginComplete(const FString& InPlayerName, const FVoiceChatResult& Result);
	void HandleVoiceChatJoinChannelComplete(const FString& ChannelName, const FVoiceChatResult& Result);

	// Plugin user handle from IVoiceChat::CreateUser(). Shared across Login/Join/Leave/Logout.
	IVoiceChatUser* VoiceChatUser = nullptr;

	// Credentials received from the server RPC, cached for the async Connect -> Login -> Join chain.
	FString CurrentVoiceRoomName;
	FString CurrentVoiceToken;
	FString CurrentVoiceClientBaseUrl;

	// Tutorial 9 (anti-cheat client):
	// Start/stop the plugin's client-side AntiCheat session. Only runs on the local controller.
	void BeginAntiCheatClientSession();
	void EndAntiCheatClientSession();

	// Bound to IEOSAntiCheatClient::OnMessageToServer; forwards bytes to the server via Server_AntiCheatMessage.
	void HandleAntiCheatMessageToServer(const TArray<uint8>& Bytes);
	FDelegateHandle AntiCheatMessageToServerHandle;
#endif

#if P2PMODE
	// Hardcoded name for the lobby.
	FName LobbyName = "LobbyName";
	// Function to create an EOS session.
	void CreateLobby(FName KeyName = "KeyName", FString KeyValue = "KeyValue");

	// Callback function. This function will run when creating the session completes.
	void HandleCreateLobbyCompleted(FName EOSLobbyName, bool bWasSuccessful);

	// Delegate to bind callback event for session creation.
	FDelegateHandle CreateLobbyDelegateHandle;

	// Function used to setup our listeners to lobby notification events - example on participant change only.
	void SetupNotifications();

	// Tutorial 7: In UE 5.2+ the single participant-change delegate was split into separate join/leave delegates.
	// Callback function. This function will run when a lobby participant joins.
	void HandleParticipantJoined(FName EOSLobbyName, const FUniqueNetId& NetId);

	// Delegate to bind callback event for participant joined.
	FDelegateHandle ParticipantJoinedDelegateHandle;

	// Callback function. This function will run when a lobby participant leaves. Leave reason indicates why
	// (e.g. Left, Disconnected, Kicked, Closed) - logging is kept simple here to focus on the OSS flow.
	void HandleParticipantLeft(FName EOSLobbyName, const FUniqueNetId& NetId, EOnSessionParticipantLeftReason LeaveReason);

	// Delegate to bind callback event for participant left.
	FDelegateHandle ParticipantLeftDelegateHandle;

	// Tutorial 9 (P2P branch): mesh AC - every peer registers every other peer.
	// Each SDK fires OnPeerActionRequired independently; lobby host KickPlayers the offender.
	//
	// StartAntiCheatPeerSession is the one-shot trigger from HandleLoginCompleted
	// (after the local NetId resolves). Bind/Unbind are the per-PC delegate hooks
	// driven from BeginPlay / EndPlay, including the post-travel rebind on joiners.
	void StartAntiCheatPeerSession();
	void BindAntiCheatViolationDelegate();
	void UnbindAntiCheatViolationDelegate();
	void HandleAntiCheatViolationP2P(const FUniqueNetIdPtr& OffendingPlayer, const FString& Reason);
	FDelegateHandle AntiCheatViolationDelegateHandle;

	// Tutorial 9 (P2P branch): catches participants already in the lobby on join,
	// which the post-join CopyLobbyData fire populates into MemberSettings.
	void HandleSessionSettingsUpdated(FName SessionName, const FOnlineSessionSettings& UpdatedSettings);
	FDelegateHandle SessionSettingsUpdatedDelegateHandle;

	// Tutorial 9 (P2P branch): catches LATER-arriving peers the OSS routes through
	// SettingsUpdated rather than ParticipantJoined (UpdateOrAddLobbyMember -
	// !bWasLobbyMemberAdded path). HandleParticipantJoined misses those.
	void HandleParticipantSettingsUpdated(FName SessionName, const FUniqueNetId& ParticipantId, const FOnlineSessionSettings& Settings);
	FDelegateHandle ParticipantSettingsUpdatedDelegateHandle;
#endif
};
