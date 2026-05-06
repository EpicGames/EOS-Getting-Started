// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"  // Tutorial 5: EOnJoinSessionCompleteResult::Type in HandleJoinSessionCompleted signature (UE enum-namespace pattern can't be forward-declared).
#include "OnlineSessionSettings.h"              // Tutorial 5: FOnlineSessionSearchResult - used by value in TOptional<...> SessionToJoin member.
#include "Interfaces/OnlineStoreInterfaceV2.h"  // Tutorial 3: FUniqueOfferId in QueryStoreOffers signatures.
#include "Interfaces/OnlinePurchaseInterface.h" // Tutorial 3: FPurchaseReceipt + FOnlineError in CheckoutOffer/QueryStoreReceipts signatures.
#include "Interfaces/OnlinePresenceInterface.h" // Tutorial 6: FOnlineUserPresence - used by TSharedRef in HandlePresenceReceived signature.
#include "EOSPlayerController.generated.h"

/** New log category for this tutorial. Bump verbosity with `log LogEOSOSSTutorial Verbose` (or VeryVerbose) to see success-path messages. */
DECLARE_LOG_CATEGORY_EXTERN(LogEOSOSSTutorial, Log, All);

// Forward declarations
class FOnlineSessionSearch;
class IVoiceChatUser;

#if !P2PMODE
// Dedicated-server-only voice types - the explicit Connect/Login/JoinChannel chain in the .cpp uses these.
class IVoiceChat;
struct FVoiceChatResult;
#endif

/**
 * Child class of APlayerController to hold player related OSS code.
 */

UCLASS()
class EOS_OSS_TUTORIAL_API AEOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Class constructor. We won't use this in this tutorial.
	AEOSPlayerController();

	// Function to load the position of the character from Player Data Storage. Real games would use a derived USaveGame; this is a pseudo-save example.
	void LoadGame(const TArray<uint8>& LoadData);

	// Function to save the position of the character to Player Data Storage. Real games would use a derived USaveGame; this is a pseudo-save example.
	void SaveGame();

protected:
	/* =============== Tutorial 1 - Login ============================= */

	// Function called when play begins.
	virtual void BeginPlay();

	// Function called when play ends. Used to clear all async-OSS delegate handles so multicast lists in the OSS interfaces don't retain stale bindings after the controller is destroyed.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Function to sign into EOS.
	void Login();

	// Callback function. Ran when signing into EOS completes.
	void HandleLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	// Delegate handle for login.
	FDelegateHandle LoginDelegateHandle;

	// Sign out of EOS. Real games typically logout on sign-out UI and app exit. This project only logs out on app exit.
	void Logout();

	// Callback function. Ran when signing out of EOS completes.
	void HandleLogoutCompleted(int32 LocalUserNum, bool bWasSuccessful);

	// Delegate handle for logout.
	FDelegateHandle LogoutDelegateHandle;

	/* =============== Tutorial 2 - Achievements, Leaderboards and Stats ========================= */

	// Code is in EOSPlayerState and EOS_OSS_TutorialCharacter

	/* =============== Tutorial 3 - Ecom ============================= */

	// Function to query EGS Game Catalog offers.
	void QueryStoreOffers();

	// Callback function. Ran when QueryStoreOffers completes.
	void HandleQueryStoreOffersCompleted(bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& Error);

	// Function to query player receipts (prior purchases). bRestoreReceipts=true forces a backend re-fetch instead of returning OSS's cached list.
	void QueryStoreReceipts(bool bRestoreReceipts = false);

	// Callback function. Ran when QueryStoreReceipts completes.
	void HandleQueryStoreReceiptsCompleted(const FOnlineError& Error);

	// Function to open the EGS checkout overlay. Result fires asynchronously when the player closes it. REQUIRES a packaged Win64 client as the EOS overlay isn't available in PIE.
	void CheckoutOffer(const FString& OfferId, int32 Quantity = 1, bool bConsumable = true);

	// Callback function. Ran when CheckoutOffer completes (overlay closed by the player).
	void HandleCheckoutCompleted(const FOnlineError& Error, const TSharedRef<FPurchaseReceipt>& Receipt);

	// Function to query player entitlements.
	void QueryEntitlements();

	// Callback function. Ran when QueryEntitlements completes.
	void HandleQueryEntitlementsCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Namespace, const FString& Error);

	// Delegate handle for QueryEntitlements.
	FDelegateHandle QueryEntitlementsDelegateHandle;

	// Function to redeem (consume) a granted entitlement, incrementing ConsumedCount on the EOS backend. Should be called on a trusted backend in production. A tampered client could replay this call and have the server grant items free. Recommended pattern: a server RPC that (1) grants the in-game item, then (2) redeems the entitlement.
	void RedeemEntitlement(const FString& EntitlementId);

	/* =============== Tutorial 4 - Lobbies & P2P ========================= */

	// Code is below in the P2PMode preprocessor guard
	
	/* =============== Tutorial 5 - Sessions ============================= */

	// Function to find EOS sessions. Hardcoded attribute key/value pair to keep things simple.
	void FindSessions(FName SearchKey = "KeyName", FString SearchValue = "KeyValue");

	// Callback function. Ran when FindSessions completes.
	void HandleFindSessionsCompleted(bool bWasSuccessful, TSharedRef<FOnlineSessionSearch> Search);

	// Delegate handle for FindSessions.
	FDelegateHandle FindSessionsDelegateHandle;

	// Connection string for the client to connect to the dedicated server.
	FString ConnectString;

	// Stores a *copy* of the chosen search result from the find-sessions callback. It must be a value (not a pointer into the search-result array): the TSharedRef<FOnlineSessionSearch> that owns that array is released at the end of HandleFindSessionsCompleted, after which any pointer into its SearchResults would dangle.
	TOptional<FOnlineSessionSearchResult> SessionToJoin;

	// Function to join the EOS session.
	void JoinSession();

	// Callback function. Ran when JoinSession completes.
	void HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	// Delegate handle for JoinSession.
	FDelegateHandle JoinSessionDelegateHandle;

	// Function to send a session/lobby invite.
	void SendSessionInvite(const FUniqueNetIdRef& Target);

	// Callback function. Ran when an invite the local user accepted (via overlay or TestAcceptLastInvite).
	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	// Delegate handle for invite-accepted notifications.
	FDelegateHandle SessionInviteAcceptedDelegateHandle;

	// Callback function. Ran when an invite to the local user is received from another player.
	void HandleSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult);

	// Delegate handle for invite-received notifications.
	FDelegateHandle SessionInviteReceivedDelegateHandle;

	// Cached most-recent received invite. Populated by HandleSessionInviteReceived, consumed by TestAcceptLastInvite. Lets us demo the invite-accept path from the console without relying on the EOS Social Overlay UI.
	TOptional<FOnlineSessionSearchResult> LastReceivedInvite;

	// Callback function. Ran when IOnlineSession::UpdateSession completes. Shared between Tutorial 5 (server-mode session attribute updates) and Tutorial 4 (P2P lobby attribute / member-attribute updates). Per-call binding; defensively cleared in EndPlay.
	void HandleUpdateSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate handle for UpdateSession.
	FDelegateHandle UpdateSessionDelegateHandle;

	/* =============== Tutorial 6 - Presence + Friends + External UI ============================= */

	// Function to publish the local user's rich-text status to EOS Presence. The EOS Social Overlay reads the rich-text/state pair and shows it on friend rows. Layered above the session-attached presence configured in Tutorial 4/5 (the bUsesPresence join-time flip).
	void SetGamePresence(const FString& StatusText);

	// Callback function. Ran when SetGamePresence completes.
	void HandleSetPresenceCompleted(const FUniqueNetId& UserId, bool bWasSuccessful);

	// Function to query a remote player's Presence.
	void QueryPresenceFor(const FUniqueNetIdRef& Target);

	// Callback function. Ran when QueryPresenceFor completes.
	void HandleQueryPresenceCompleted(const FUniqueNetId& UserId, bool bWasSuccessful);

	// Multicast callback. Fires whenever the SDK pushes a presence update for any user (after a query response, or when a friend's status changes).
	void HandlePresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence);

	// Delegate handle for presence-received notifications.
	FDelegateHandle PresenceReceivedDelegateHandle;

	// Function to read the local user's friends list. Pairs with Presence (query a friend's PUID for their rich-text status) and UserInfo (resolve PUID -> display name).
	void ReadFriendsList();

	// Callback function. Ran when ReadFriendsList completes.
	void HandleReadFriendsListCompleted(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

	// Multicast callback. Fires when the friends list changes (friend added / removed / blocked / status update).
	void HandleFriendsChange();

	// Delegate handle for friend-change notifications.
	FDelegateHandle FriendsChangeDelegateHandle;

	// Function to programmatically open the EOS Social Overlay to the Friends list. NOTE: OSS-EOS only implements ShowFriendsUI; ShowProfileUI / ShowInviteUI are stubbed because the EOS SDK has no equivalent entry points.
	void ShowFriendsOverlay();

	/* =============== Tutorial 7 - Title + Player Data Storage ============================= */

	// Function to enumerate Title File Storage and read the 1st returned file. Title files are server-authored, read-only.
	void LoadTitleData();

	// Callback function. Ran when title-file enumeration completes.
	void HandleEnumTitleFilesCompleted(bool bWasSuccessfull, const FString& Error);

	// Delegate handle for title-file enumeration.
	FDelegateHandle EnumTitleFilesDelegateHandle;

	// Callback function. Ran when an individual title file has been read. Bound from HandleEnumTitleFilesCompleted.
	void HandleReadTitleFileCompleted(bool bWasSuccessfull, const FString& FileName);

	// Delegate handle for title-file read.
	FDelegateHandle ReadTitleFileDelegateHandle;

	// Function to write to Player Data Storage. Called when the player quits and the game is saved.
	void WritePlayerDataStorage(FString FileName, TArray<uint8> FileData);

	// Callback function. Ran when WritePlayerDataStorage completes.
	void HandleWritePlayerDataStorageCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName);

	// Delegate handle for WritePlayerDataStorage.
	FDelegateHandle WritePlayerDataStorageDelegateHandle;

	// Function to enumerate the local user's Player Data Storage files and read the 1st returned file.
	void LoadPlayerData();

	// Callback function. Ran when player-file enumeration completes.
	void HandleEnumPlayerFilesCompleted(bool bWasSuccessfull, const FUniqueNetId& NetId);

	// Delegate handle for player-file enumeration.
	FDelegateHandle EnumPlayerFilesDelegateHandle;

	// Callback function. Ran when an individual player data file has been read. Bound from HandleEnumPlayerFilesCompleted.
	void HandleReadPlayerFileCompleted(bool bWasSuccessfull, const FUniqueNetId& UserId, const FString& FileName);

	// Delegate handle for player-file read.
	FDelegateHandle ReadPlayerDataFileDelegateHandle;

	/* =============== Tutorial 8 - Player Reports ============================= */

	// Function to send a player report. Surfaced via Exec command (TestSendPlayerReport). Real games invoke this from a post-match report UI, never silently on every departure.
	void SendPlayerReport(const FUniqueNetIdRef& Reporter, const FUniqueNetIdRef& Reported, const FString& Message, const FString& ContextJson);

	/* =============== Tutorial 9 - Sanctions ============================= */

	// Function to query the local user's active sanctions. Sets bRestrictMatchmaking on RESTRICT_MATCHMAKING (a standard EOS action; developers can also define custom ones).
	void GetSanctions(const FUniqueNetId& UserId);

	// Set by GetSanctions; FindSessions early-returns when true.
	bool bRestrictMatchmaking = false;

	// Function to submit an appeal for a sanction by ReferenceId. Hardcoded reason - real games bind a dropdown to EPlayerSanctionAppealReason.
	void CreateSanctionAppeal(const FUniqueNetId& UserId, FString ReferenceId);

	/* =============== Test* exec wrappers (in-game console: ~) ============================= */
	// Manual triggers for the hot paths that can't auto-fire. These are console commands called by pressing the ~ button in-game.
	//
	// Usage in the in-game console (`~`):
	//   TestUnlockAchievement <AchievementId>
	//     - directly unlocks an achievement on the EOS backend (Tutorial 2);
	//       use for achievements not driven by a stat threshold. Copy the
	//       AchievementId from the EOS Dev Portal achievement definition.
	//   TestCheckoutOffer <OfferId>
	//   TestRedeemEntitlement <EntitlementId>
	//   TestSendPlayerReport <Message> [ContextJson]
	//     - defaults target to first non-self player in the local named session
	//       (LobbyName for P2P, NAME_GameSession for server)
	//   TestCreateSanctionAppeal <ReferenceId>
	//     - copy ReferenceId from the active sanction in Dev Portal
	//   TestSendSessionInvite <ProductUserId>
	//     - copy the target PUID from the other client's login log
	//   TestAcceptLastInvite
	//     - bypasses the EOS Social Overlay accept popup; replays the
	//       most recent cached InviteResult through JoinSession
	//   TestQueryPresence <ProductUserId>
	//     - async query of a remote user's presence; result lands in
	//       HandlePresenceReceived + a one-shot HandleQueryPresenceCompleted
	//   TestSetGamePresence <StatusText>
	//     - publishes StatusText as the local user's rich-text presence;
	//       EGS Social Overlay shows it on friend rows as
	//       "EOS_OSS_Tutorial - <StatusText>"
	//   TestReadFriends
	//     - fetches the local user's friends list ("default" filter);
	//       on success logs each friend's id + cached presence summary
	//   TestShowFriendsOverlay
	//     - opens the EOS Social Overlay to the Friends list view.
	//       Profile/invite overlay views are user-navigated only -
	//       OSS-EOS does not expose programmatic open for those.
	//   TestLogout
	//     - signs the local user out of EOS Game Services. Pairs
	//       with Login from Tutorial 1.
	//   TestSetLobbyAttribute <Key> <Value>       (P2P only)
	//     - host updates the lobby's overall Settings map. Joiners
	//       calling this get an OSS error since they don't own the lobby.
	//   TestSetMyMemberAttribute <Key> <Value>    (P2P only)
	//     - any P2P member updates their own per-member attribute
	//       (MemberSettings). Useful for team / ready / class state.
	//
	// Companion CLI flag (launch-time, not console):
	//   -NoAutoJoin  - skip the post-login auto-find/join chain so the
	//                  invite path is the only way to enter a session.
	//                  Pair with TestSendSessionInvite from the host.
	UFUNCTION(Exec)
	void TestUnlockAchievement(const FString& AchievementId);

	UFUNCTION(Exec)
	void TestCheckoutOffer(const FString& OfferId);

	UFUNCTION(Exec)
	void TestRedeemEntitlement(const FString& EntitlementId);

	UFUNCTION(Exec)
	void TestSendPlayerReport(const FString& Message, const FString& Context = TEXT(""));

	UFUNCTION(Exec)
	void TestCreateSanctionAppeal(const FString& ReferenceId);

	UFUNCTION(Exec)
	void TestSendSessionInvite(const FString& TargetProductUserId);

	UFUNCTION(Exec)
	void TestAcceptLastInvite();

	UFUNCTION(Exec)
	void TestQueryPresence(const FString& TargetProductUserId);

	UFUNCTION(Exec)
	void TestSetGamePresence(const FString& StatusText);

	UFUNCTION(Exec)
	void TestReadFriends();

	UFUNCTION(Exec)
	void TestShowFriendsOverlay();

	UFUNCTION(Exec)
	void TestLogout();

	UFUNCTION(Exec)
	void TestSetLobbyAttribute(const FString& Key, const FString& Value);

	UFUNCTION(Exec)
	void TestSetMyMemberAttribute(const FString& Key, const FString& Value);

public:
	/* =============== RPCs ============================= */
	// Declared outside the mode #if guards because UHT can't see preprocessor blocks; the _Implementation bodies are #if-guarded in the .cpp. These RPCs are used in Tutorial 10 and 11 (code below).

	// Tutorial 10: Client -> server RPC. Joiner signals load-complete + sends its EOS Connect IdToken for VerifyIdToken. WithValidation rejects empty / oversize JWTs before the SDK call - the joiner controls this FString and a misbehaving client could otherwise blast huge strings at the authority.
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_NotifyAntiCheatReady(const FString& IdTokenJwt);

	// Tutorial 10: Client -> server RPC. Opaque EOS AntiCheat byte relay (server-bound).
	UFUNCTION(Server, Reliable)
	void Server_AntiCheatMessage(const TArray<uint8>& Bytes);

	// Tutorial 10: Server -> client RPC. Opaque EOS AntiCheat byte relay (client-bound).
	UFUNCTION(Client, Reliable)
	void Client_AntiCheatMessage(const TArray<uint8>& Bytes);

	// Tutorial 10 (P2P branch): Client -> server RPC. Non-host's violation report - host logs for telemetry only (host's own SDK fires independently and does the actual kick).
	UFUNCTION(Server, Reliable)
	void Server_PeerViolationDetected(const FUniqueNetIdRepl& OffendingPlayer, const FString& Reason);

	// Tutorial 11: Server -> client RPC. Per-player EOS RTC room name + participant token + media-server URL.
	UFUNCTION(Client, Reliable)
	void Client_ReceiveVoiceCredentials(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl);

protected:
	/* =============== Tutorial 10 - Anti-cheat (cross-mode handshake) ============================= */

	// Function to copy the joiner's EOS Connect IdToken and RPC it to the authority for VerifyIdToken before RegisterClient (!P2PMODE) / RegisterPeer (P2PMODE).
	void SendIdTokenForVerification();

	/* =============== Tutorial 11 - Voice (cross-mode speaking-state notification) ============================= */

	// Function to fetch the active IVoiceChatUser. Mode-aware: explicit IVoiceChat::CreateUser one in P2PMODE=0 (member VoiceChatUser); OSS-managed lobby voice user in P2PMODE=1. See banner in the .cpp for full context.
	IVoiceChatUser* GetActiveVoiceChatUser() const;

	// Function to subscribe HandleVoiceChatPlayerTalkingUpdated to the active user's OnVoiceChatPlayerTalkingUpdated multicast. Idempotent; called from multiple "voice is plausibly ready" hooks.
	void BindVoiceChatPlayerTalkingNotification();

	// Multicast callback. Fires when any participant's talking state changes - logs only.
	void HandleVoiceChatPlayerTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking);

	// Set after BindVoiceChatPlayerTalkingNotification successfully subscribes. Prevents double-binding from the multiple call sites.
	bool bVoiceTalkingNotificationBound = false;

	// Delegate handle for the speaking-state notification. The OSS-managed IVoiceChatUser outlives this PC (P2PMODE=1 lobby-voice path), so we clear the binding explicitly in EndPlay rather than relying on AddUObject's GC cleanup.
	FDelegateHandle VoiceChatPlayerTalkingDelegateHandle;

#if !P2PMODE
protected:
	/* =============== Tutorial 11 - Voice on server (P2PMODE=0 only) ============================= */

	// Per-session voice-chat opt-in. Checked when the server-issued credentials RPC arrives.
	bool bVoiceChatEnabled = true;

	// Function to teardown the voice session. Chains LeaveChannel -> Logout -> ReleaseUser -> Disconnect. Called from EndPlay.
	void LeaveVoiceChat();

	// Callback function. Ran when the voice Connect step completes.
	void HandleVoiceChatConnectComplete(const FVoiceChatResult& Result);

	// Callback function. Ran when the voice Login step completes.
	void HandleVoiceChatLoginComplete(const FString& InPlayerName, const FVoiceChatResult& Result);

	// Callback function. Ran when the voice JoinChannel step completes.
	void HandleVoiceChatJoinChannelComplete(const FString& ChannelName, const FVoiceChatResult& Result);

	// Plugin user handle from IVoiceChat::CreateUser(). Shared across Login / Join / Leave / Logout.
	IVoiceChatUser* VoiceChatUser = nullptr;

	// Credentials received from the server RPC, cached for the async Connect -> Login -> Join chain.
	FString CurrentVoiceRoomName;
	FString CurrentVoiceToken;
	FString CurrentVoiceClientBaseUrl;

#if ACMODE
	/* =============== Tutorial 10 - Anti-cheat client (P2PMODE=0 + ACMODE=1) ============================= */

	// Function to start the plugin's client-side AntiCheat session. Only runs on the local controller.
	void BeginAntiCheatClientSession();

	// Function to end the plugin's client-side AntiCheat session.
	void EndAntiCheatClientSession();

	// Callback function. Ran when the SDK has bytes to send to the server. Forwards to Server_AntiCheatMessage.
	void HandleAntiCheatMessageToServer(const TArray<uint8>& Bytes);

	// Delegate handle for the AC message-to-server notification.
	FDelegateHandle AntiCheatMessageToServerHandle;
#endif // ACMODE
#endif // !P2PMODE

#if P2PMODE
	/* =============== Tutorial 4 - Lobbies (P2PMODE=1 only) ============================= */

	// Hardcoded name for the lobby.
	FName LobbyName = "LobbyName";

	// Function to create an EOS lobby.
	void CreateLobby(FName KeyName = "KeyName", FString KeyValue = "KeyValue");

	// Callback function. Ran when CreateLobby completes.
	void HandleCreateLobbyCompleted(FName EOSLobbyName, bool bWasSuccessful);

	// Delegate handle for CreateLobby.
	FDelegateHandle CreateLobbyDelegateHandle;

	// Function to setup our listeners for lobby notification events.
	void SetupNotifications();

	// Callback function. Ran when a lobby participant joins.
	void HandleParticipantJoined(FName EOSLobbyName, const FUniqueNetId& NetId);

	// Delegate handle for participant-joined notifications.
	FDelegateHandle ParticipantJoinedDelegateHandle;

	// Callback function. Ran when a lobby participant leaves. Leave reason indicates why (e.g. Left, Disconnected, Kicked, Closed) - logging is kept simple here to focus on the OSS flow.
	void HandleParticipantLeft(FName EOSLobbyName, const FUniqueNetId& NetId, EOnSessionParticipantLeftReason LeaveReason);

	// Delegate handle for participant-left notifications.
	FDelegateHandle ParticipantLeftDelegateHandle;

	// Callback function. Ran when the lobby's overall Settings map changes (post-join CopyLobbyData populates participants already in the lobby into MemberSettings - without this, late-joiners miss already-present peers).
	void HandleSessionSettingsUpdated(FName SessionName, const FOnlineSessionSettings& UpdatedSettings);

	// Delegate handle for session-settings-updated notifications.
	FDelegateHandle SessionSettingsUpdatedDelegateHandle;

	// Callback function. Ran when an individual participant's MemberSettings change. Catches LATER-arriving peers the OSS routes through SettingsUpdated rather than ParticipantJoined (UpdateOrAddLobbyMember - !bWasLobbyMemberAdded path).
	void HandleParticipantSettingsUpdated(FName SessionName, const FUniqueNetId& ParticipantId, const FOnlineSessionSettings& Settings);

	// Delegate handle for participant-settings-updated notifications.
	FDelegateHandle ParticipantSettingsUpdatedDelegateHandle;

#if ACMODE
	/* =============== Tutorial 10 - Anti-cheat peer (P2PMODE=1 + ACMODE=1) ============================= */

	// Function to start the AntiCheat peer session. One-shot trigger from HandleLoginCompleted (after the local NetId resolves).
	void StartAntiCheatPeerSession();

	// Function to bind the per-PC violation delegate. Called from BeginPlay including the post-travel rebind on joiners.
	void BindAntiCheatViolationDelegate();

	// Function to unbind the per-PC violation delegate. Called from EndPlay.
	void UnbindAntiCheatViolationDelegate();

	// Multicast callback. Fires when the AC SDK reports a violation. Each peer's SDK fires independently; the lobby host KickPlayers the offender.
	void HandleAntiCheatViolationP2P(const FUniqueNetIdPtr& OffendingPlayer, const FString& Reason);

	// Delegate handle for the AC violation notification.
	FDelegateHandle AntiCheatViolationDelegateHandle;
#endif // ACMODE
#endif // P2PMODE
};
