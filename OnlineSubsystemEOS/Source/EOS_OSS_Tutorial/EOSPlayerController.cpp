// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSPlayerController.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSessionSettings.h" // UE 5.8: FOnlineSessionSearchResult is only forward-declared by OnlineSessionInterface.h - include the full definition here.
#include "Online/OnlineSessionNames.h" // SEARCH_LOBBIES FName used by the P2P find-lobby branch (no longer pulled in transitively in UE 5.8).
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineEntitlementsInterface.h" // Tutorial 3: IOnlineEntitlements + FOnlineEntitlement.
#include "IOnlineSubsystemEOS.h"                    // Tutorial 8: IOnlineSubsystemEOS for the EOS-only IOnlinePlayerReportEOS getter.
#include "Interfaces/OnlinePlayerReportEOSInterface.h" // Tutorial 8: IOnlinePlayerReportEOS - EOS-specific, no generic OSS abstraction.
#include "Interfaces/OnlinePlayerSanctionEOSInterface.h" // Tutorial 9: IOnlinePlayerSanctionEOS - EOS-specific, no generic OSS abstraction.
#include "Interfaces/OnlinePresenceInterface.h" // Tutorial 6: IOnlinePresence - generic OSS interface, EOS-backed via OSS-EOS plugin.
#include "Interfaces/OnlineFriendsInterface.h" // Tutorial 6: IOnlineFriends - read friends list, friend change notifications.
#include "Interfaces/OnlineExternalUIInterface.h" // Tutorial 6: IOnlineExternalUI - programmatically open EOS Social Overlay views.
#include "Serialization/JsonWriter.h"               // Tutorial 8: building the optional JSON Context blob for the report.
#include "Policies/CondensedJsonPrintPolicy.h"      // Tutorial 8: compact JSON output policy used by TJsonWriter above.
#include "Containers/Array.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "EOS_OSS_TutorialGameMode.h"

// Tutorial 10 (anti-cheat): only included when ACMODE=1. The plugin's headers
// pull in EOS SDK declarations - skip them entirely when the AC wiring is off
// so ACMODE=0 builds don't carry unused symbol references.
#if ACMODE
#include "IEOSAntiCheat.h"
#include "IEOSAntiCheatClient.h"
#endif
#include "EOSGameSession.h"
#include "GameFramework/PlayerState.h"       // APlayerState::GetUniqueId().
#include "GameFramework/GameSession.h"       // AGameSession::KickPlayer.

// VoiceChat.h is shared by both modes - P2PMODE=1 uses IVoiceChatUser via the OSS lobby voice
// path (see GetActiveVoiceChatUser below) for the speaking-state notification subscription.
#include "VoiceChat.h"                      // IVoiceChat, IVoiceChatUser, FVoiceChatResult, EVoiceChatChannelType, all the FOnVoiceChat*CompleteDelegate typedefs.

#if !P2PMODE
#include "EOSVoiceChat.h"                   // FEOSVoiceChat::SetPlatformHandle() - used to reuse OSS's EOS platform rather than letting the voice plugin create a second one.
#include "EOSVoiceChatTypes.h"               // FEOSVoiceChatChannelCredentials::ToJson - we build the channel-credentials blob by hand from the server-supplied token + url.
#include "IOnlineSubsystemEOS.h"             // IOnlineSubsystemEOS::GetEOSPlatformHandle() - source of the platform handle we hand to the voice plugin.
#include "OnlineSubsystemEOSTypesPublic.h"  // IUniqueNetIdEOS - exposes GetProductUserId() on OSS-produced FUniqueNetIds.
#include "EOSShared.h"                       // LexToString(EOS_ProductUserId) - formats the PUID as the string IVoiceChatUser::Login wants.
#include "Engine/LocalPlayer.h"              // ULocalPlayer::GetPlatformUserId() for the voice Login() call.
#if ACMODE
#include "IEOSAntiCheatServer.h"             // Server-mode RPC impl feeds bytes back via IEOSAntiCheatServer.
#endif
#endif

DEFINE_LOG_CATEGORY(LogEOSOSSTutorial);


AEOSPlayerController::AEOSPlayerController()
{
    // Tutorial 1: Including constructor here for clarity. Nothing added in derived class for this tutorial.
}

void AEOSPlayerController::BeginPlay()
{
    // Tutorial 1: On BeginPlay call our login function. This is only on the GameClient, not on the DedicatedServer.
    Super::BeginPlay();

    if (!IsRunningDedicatedServer())
    {
        Login(); // Call login function only on the client.

        // Tutorial 5 + 7: Bind the OSS invite delegates once per controller.
        // Mode-agnostic - the same delegates fire for server-mode sessions
        // and P2P lobbies, and for both Exec-driven and overlay-driven
        // invites. Cleared in EndPlay alongside the other OSS handles.
        IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
        IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
        if (Session.IsValid())
        {
            SessionInviteAcceptedDelegateHandle =
                Session->AddOnSessionUserInviteAcceptedDelegate_Handle(
                    FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSessionUserInviteAccepted));
            SessionInviteReceivedDelegateHandle =
                Session->AddOnSessionInviteReceivedDelegate_Handle(
                    FOnSessionInviteReceivedDelegate::CreateUObject(this, &ThisClass::HandleSessionInviteReceived));
        }

        // Tutorial 6: Bind OnPresenceReceived. Multicast - fires for any
        // user whose presence changed (after a query, or unsolicited from
        // the SDK pushing friend updates).
        IOnlinePresencePtr Presence = Subsystem ? Subsystem->GetPresenceInterface() : nullptr;
        if (Presence.IsValid())
        {
            PresenceReceivedDelegateHandle =
                Presence->AddOnPresenceReceivedDelegate_Handle(
                    FOnPresenceReceivedDelegate::CreateUObject(this, &ThisClass::HandlePresenceReceived));
        }

        // Tutorial 6: Friends notification bind. Per-LocalUserNum so we
        // bind for index 0 (the only local user).
        IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
        if (Friends.IsValid())
        {
            FriendsChangeDelegateHandle =
                Friends->AddOnFriendsChangeDelegate_Handle(0,
                    FOnFriendsChangeDelegate::CreateUObject(this, &ThisClass::HandleFriendsChange));
        }

        // Tutorial 11: opportunistic speaking-state-notification bind. In P2PMODE=1 the OSS lobby
        // voice path may already have set up the voice user by now (e.g. on the joiner post-travel,
        // where the lobby was joined before this controller was respawned). Idempotent - if voice
        // isn't ready yet, the join-channel / lobby-create hooks will retry.
        BindVoiceChatPlayerTalkingNotification();
    }

#if ACMODE
    // Tutorial 10: post-travel JWT send (cross-mode). The local non-host controller sends its
    // EOS Connect IdToken to the authority (dedicated server in !P2PMODE, listen-server host
    // in P2PMODE) for VerifyIdToken before RegisterClient / RegisterPeer.
    if (IsLocalController() && !HasAuthority())
    {
        SendIdTokenForVerification();
    }
#endif

#if !P2PMODE
#if ACMODE
    if (IsLocalController())
    {
        // Tutorial 10: Rebind the AC SDK's OnMessageToServer delegate to this live
        // PC (the pre-travel bind from HandleLoginCompleted is now stale). BeginSession is idempotent.
        BeginAntiCheatClientSession();
    }
#endif
#else
    if (IsLocalController())
    {
#if ACMODE
        // Tutorial 10 (P2P branch): bind the violation delegate to this live PC.
        // First-time spawns and post-travel rebinds both go through here - on the
        // joiner, ClientTravel destroyed the pre-travel PC and its delegate handle
        // is already gone (EndPlay -> UnbindAntiCheatViolationDelegate), so the
        // post-travel PC needs a fresh bind. Session start happens later from
        // HandleLoginCompleted via StartAntiCheatPeerSession.
        BindAntiCheatViolationDelegate();
#endif

        IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
        IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
        if (Session.IsValid() && Session->GetNamedSession(LobbyName))
        {
            SetupNotifications();
        }
    }
#endif
}

void AEOSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !P2PMODE
    // Tutorial 11: Kick off voice teardown before clearing OSS delegates. Runs async - the chain
    // continues after EndPlay returns.
    LeaveVoiceChat();

    // Tutorial 10: AC session is plugin-scoped and shuts down with the module, not here.
#else
#if ACMODE
    // Tutorial 10 (P2P branch): clear violation binding. Plugin shutdown handles the AC session itself.
    UnbindAntiCheatViolationDelegate();
#endif
#endif

    // Clear every delegate handle this controller bound on the OSS interfaces. Without this the OSS
    // multicast lists retain stale handles pointing at our (soon-to-be-GC'd) UObject. The
    // CreateUObject bindings themselves are weak and will no-op, but letting them accumulate leaks
    // slots and can confuse shutdown ordering.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    if (Subsystem)
    {
        IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
        if (Identity.IsValid())
        {
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
            LoginDelegateHandle.Reset();
            // Tutorial 1: Logout delegate handle (defensive clear).
            if (LogoutDelegateHandle.IsValid())
            {
                Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
                LogoutDelegateHandle.Reset();
            }
        }

        IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
        if (Session.IsValid())
        {
            Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
            FindSessionsDelegateHandle.Reset();
            Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
            JoinSessionDelegateHandle.Reset();
            // Tutorial 5 + 4 + 7: UpdateSession completion (per-call bind;
            // defensive clear here in case a call was in flight).
            if (UpdateSessionDelegateHandle.IsValid())
            {
                Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
                UpdateSessionDelegateHandle.Reset();
            }
            // Tutorial 5 + 7: Invite delegates bound in BeginPlay (client-only).
            if (SessionInviteAcceptedDelegateHandle.IsValid())
            {
                Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedDelegateHandle);
                SessionInviteAcceptedDelegateHandle.Reset();
            }
            if (SessionInviteReceivedDelegateHandle.IsValid())
            {
                Session->ClearOnSessionInviteReceivedDelegate_Handle(SessionInviteReceivedDelegateHandle);
                SessionInviteReceivedDelegateHandle.Reset();
            }
#if P2PMODE
            Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyDelegateHandle);
            CreateLobbyDelegateHandle.Reset();
            Session->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantJoinedDelegateHandle);
            ParticipantJoinedDelegateHandle.Reset();
            Session->ClearOnSessionParticipantLeftDelegate_Handle(ParticipantLeftDelegateHandle);
            ParticipantLeftDelegateHandle.Reset();
            // Both stay live for the session lifetime - always clear here.
            if (SessionSettingsUpdatedDelegateHandle.IsValid())
            {
                Session->ClearOnSessionSettingsUpdatedDelegate_Handle(SessionSettingsUpdatedDelegateHandle);
                SessionSettingsUpdatedDelegateHandle.Reset();
            }
            if (ParticipantSettingsUpdatedDelegateHandle.IsValid())
            {
                Session->ClearOnSessionParticipantSettingsUpdatedDelegate_Handle(ParticipantSettingsUpdatedDelegateHandle);
                ParticipantSettingsUpdatedDelegateHandle.Reset();
            }
#endif
        }

        IOnlineTitleFilePtr TitleFile = Subsystem->GetTitleFileInterface();
        if (TitleFile.IsValid())
        {
            TitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(EnumTitleFilesDelegateHandle);
            EnumTitleFilesDelegateHandle.Reset();
            TitleFile->ClearOnReadFileCompleteDelegate_Handle(ReadTitleFileDelegateHandle);
            ReadTitleFileDelegateHandle.Reset();
        }

        IOnlineUserCloudPtr UserCloud = Subsystem->GetUserCloudInterface();
        if (UserCloud.IsValid())
        {
            UserCloud->ClearOnWriteUserFileCompleteDelegate_Handle(WritePlayerDataStorageDelegateHandle);
            WritePlayerDataStorageDelegateHandle.Reset();
            UserCloud->ClearOnEnumerateUserFilesCompleteDelegate_Handle(EnumPlayerFilesDelegateHandle);
            EnumPlayerFilesDelegateHandle.Reset();
            UserCloud->ClearOnReadUserFileCompleteDelegate_Handle(ReadPlayerDataFileDelegateHandle);
            ReadPlayerDataFileDelegateHandle.Reset();
        }

        // Tutorial 3: Entitlements interface uses the delegate-list pattern,
        // unlike Store/Purchase which take callbacks per call. Clear our handle
        // so the OSS multicast list doesn't retain a stale binding.
        IOnlineEntitlementsPtr Entitlements = Subsystem->GetEntitlementsInterface();
        if (Entitlements.IsValid() && QueryEntitlementsDelegateHandle.IsValid())
        {
            Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
            QueryEntitlementsDelegateHandle.Reset();
        }

        // Tutorial 6: Presence delegate bound in BeginPlay (client-only).
        IOnlinePresencePtr Presence = Subsystem->GetPresenceInterface();
        if (Presence.IsValid() && PresenceReceivedDelegateHandle.IsValid())
        {
            Presence->ClearOnPresenceReceivedDelegate_Handle(PresenceReceivedDelegateHandle);
            PresenceReceivedDelegateHandle.Reset();
        }

        // Tutorial 6: Friends bind (also client-only).
        IOnlineFriendsPtr Friends = Subsystem->GetFriendsInterface();
        if (Friends.IsValid() && FriendsChangeDelegateHandle.IsValid())
        {
            Friends->ClearOnFriendsChangeDelegate_Handle(0, FriendsChangeDelegateHandle);
            FriendsChangeDelegateHandle.Reset();
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AEOSPlayerController::LoadGame(const TArray<uint8>& LoadData)
{
    // Tutorial 7: This function is part of the Login() callback callstack. It is only called on clients. LoadData is the serialized data retrieved from the Player Data Storage backend.
    // FVector serializes to 24 bytes (three doubles).
    if (LoadData.Num() != 24)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::LoadGame] Invalid load data. Serialized FVector should be 24 bytes. Falling back to default starting location."));
        return;
    }

    APawn* LocalPawn = GetPawn();
    if (!LocalPawn)
    {
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::LoadGame] Pawn not available yet - skipping position restore."));
        return;
    }

    // Deserialize the FVector.
    FVector InitialPlayerLocation;
    FMemoryReader MemoryReader(LoadData);
    MemoryReader << InitialPlayerLocation;
    LocalPawn->SetActorLocation(InitialPlayerLocation); // Set the initial location.
}

void AEOSPlayerController::SaveGame()
{
    /*
    Tutorial 7: Called from the Quit() function in the character class. This is a "pseudo" save game function. The purpose here is to show how to use EOS Player Data Storage.
    This is not an example on how a game should be saved. You should use a derived USaveGame class and save all data that is needed for your game.
    */
    APawn* LocalPawn = GetPawn();
    if (!LocalPawn)
    {
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::SaveGame] Pawn not available - nothing to save."));
        return;
    }

    FVector FinalPlayerLocation = LocalPawn->GetActorLocation();

    // Prepare our SaveData - serialize the FVector.
    TArray<uint8> SaveData;
    FMemoryWriter MemoryWriter(SaveData);
    MemoryWriter << FinalPlayerLocation;

    // Write to player data storage.
    WritePlayerDataStorage("CharacterPawnLocation", SaveData);
}

// =====================================================================
// Tutorial 1: Login (EAS + EOS Connect, Logout).
//
// IOnlineIdentity::Login + Logout drive sign-in/out against Epic Account
// Services and EOS Game Services. The login completion handler is also
// the project's post-login fan-out point - other tutorial subsystems
// (Ecom queries, presence/friends bind, AC client session start) hook
// off it. Mode-agnostic.
// =====================================================================

void AEOSPlayerController::Login()
{
    /*
    Tutorial 1: This function will access the EOS OSS via the OSS identity interface to log first into Epic Account Services, and then into Epic Game Services.
    It will bind a delegate to handle the callback event once login call succeeds or fails.
    All functions that access the OSS will have this structure: 1-Get OSS interface, 2-Bind delegate for callback and 3-Call OSS interface function (which will call the corresponding EOS OSS function).
    */
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;

    if (Identity.IsValid())
    {
        // If you're already logged in, don't try to login again.
        // This can happen if your player travels to a dedicated server or different maps as BeginPlay() will be called each time.
        const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
        if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
        {
            return;
        }

        /* This binds a delegate so we can run our function when the callback completes. 0 represents the player number.
           You should parametrize this Login function and pass the parameter here for splitscreen.
        */
        LoginDelegateHandle =
            Identity->AddOnLoginCompleteDelegate_Handle(
                0,
                FOnLoginCompleteDelegate::CreateUObject(
                    this,
                    &ThisClass::HandleLoginCompleted));

        // Grab command line parameters. If empty call hardcoded login function - Hardcoded login function useful for Play In Editor.
        FString AuthType;
        FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), AuthType);

        if (!AuthType.IsEmpty()) // If parameter is NOT empty we can autologin.
        {
            /* In most situations you will want to automatically log a player in using the parameters passed via CLI.
               For example, using the exchange code for the Epic Games Store.
            */
            UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::Login] Logging into EOS via AutoLogin (CLI AUTH_TYPE set)."));

            if (!Identity->AutoLogin(0))
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Login] AutoLogin call failed."));
                Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
                LoginDelegateHandle.Reset();
            }
        }
        else
        {
            /* Fallback if the CLI parameters are empty. Useful for PIE.
               The type here could be developer if using the DevAuthTool, ExchangeCode if the game is launched via the Epic Games Launcher, etc...
            */
            FOnlineAccountCredentials Credentials("AccountPortal", "", "");

            UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::Login] Logging into EOS via AccountPortal (no CLI AUTH_TYPE)."));

            if (!Identity->Login(0, Credentials))
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Login] Login call failed."));
                Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
                LoginDelegateHandle.Reset();
            }
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Login] Identity interface null"));
    }
}

void AEOSPlayerController::HandleLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    /*
    Tutorial 1: This function handles the callback from logging in. You should not proceed with any EOS features until this function is called.
    This function will remove the delegate that was bound in the Login() function.
    */
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleLoginCompleted] Login succeeded - Player %s"), *UserId.ToString());
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleLoginCompleted] Loading cloud data and searching for a session..."));

        LoadTitleData();  // Load any game-related data (in this case a string output to logs).
        LoadPlayerData(); // Load save-game data.

        // Tutorial 5 + 7: Test/debug CLI flag - skip the auto-find/join chain.
        // Lets the second client log in but stay out of the session so the
        // invite-driven path (TestSendSessionInvite from the host, accept
        // via overlay or TestAcceptLastInvite) is the only way it joins.
        if (FParse::Param(FCommandLine::Get(), TEXT("NoAutoJoin")))
        {
            UE_LOG(LogEOSOSSTutorial, Verbose,
                TEXT("[AEOSPlayerController::HandleLoginCompleted] -NoAutoJoin set; skipping GetSanctions/FindSessions chain."));
        }
        else
        {
            // Tutorial 9: Sanctions query first - GetSanctions chains to
            // FindSessions from its callback, gated by bRestrictMatchmaking.
            GetSanctions(UserId);
        }

        // Tutorial 3: Fan out the new Ecom flow's three queries. Catalog offers,
        // existing receipts, and current entitlements. See header for safety notes.
        QueryStoreOffers();
        QueryStoreReceipts();
        QueryEntitlements();

#if ACMODE
#if !P2PMODE
        // Tutorial 10: Login succeeded so the NetId is ready - start the AC session.
        BeginAntiCheatClientSession();
#else
        // Tutorial 10 (P2P branch): peers are registered later as lobby events fire.
        // Delegate is already bound from BeginPlay - we just start the session here.
        StartAntiCheatPeerSession();
#endif
#endif
    }
    else
    {
        // If your game is online only, you may want to return an error to the user and return to a menu that uses a different GameMode/PlayerController.
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleLoginCompleted] EOS login failed: %s"), *Error);
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;

    if (Identity.IsValid())
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginDelegateHandle);
        LoginDelegateHandle.Reset();
    }
}

void AEOSPlayerController::Logout()
{
    // Tutorial 1: Sign the local user out of EOS Game Services. Async,
    // mirrors Login. Real games typically call this from a sign-out
    // UI button or as part of app shutdown rather than relying on
    // process termination.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    if (!Identity.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Logout] Identity interface null."));
        return;
    }

    LogoutDelegateHandle =
        Identity->AddOnLogoutCompleteDelegate_Handle(0,
            FOnLogoutCompleteDelegate::CreateUObject(this, &ThisClass::HandleLogoutCompleted));

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::Logout] Logging out."));

    if (!Identity->Logout(0))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Logout] Logout call failed."));
        Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
        LogoutDelegateHandle.Reset();
    }
}

void AEOSPlayerController::HandleLogoutCompleted(int32 LocalUserNum, bool bWasSuccessful)
{
    // Tutorial 1: Logout completion. After this returns, GetLoginStatus(0)
    // is NotLoggedIn and any subsequent OSS calls that require auth
    // will fail. A real game would route the player back to a login
    // screen here.
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[AEOSPlayerController::HandleLogoutCompleted] Logout succeeded for local user %d."),
            LocalUserNum);
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleLogoutCompleted] Logout failed for local user %d."),
            LocalUserNum);
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    if (Identity.IsValid())
    {
        Identity->ClearOnLogoutCompleteDelegate_Handle(LocalUserNum, LogoutDelegateHandle);
        LogoutDelegateHandle.Reset();
    }
}

// =====================================================================
// Tutorial 3: Ecom (Store + Purchase + Entitlements) - new flow.
// =====================================================================

void AEOSPlayerController::QueryStoreOffers()
{
    // EOS doesn't support filtered category queries via OSS; pass an empty
    // filter to retrieve all offers.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlineStoreV2Ptr Store = Subsystem ? Subsystem->GetStoreV2Interface() : nullptr;
    if (!Identity.IsValid() || !Store.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryStoreOffers] Identity or StoreV2 interface null."));
        return;
    }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::Type::LoggedIn)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryStoreOffers] Player not logged in."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::QueryStoreOffers] Querying catalog offers..."));

    FOnlineStoreFilter EmptyFilter = {};
    Store->QueryOffersByFilter(*LocalNetId, EmptyFilter,
        FOnQueryOnlineStoreOffersComplete::CreateUObject(this, &ThisClass::HandleQueryStoreOffersCompleted));
}

void AEOSPlayerController::HandleQueryStoreOffersCompleted(bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& Error)
{
    if (!bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleQueryStoreOffersCompleted] Failed: %s"), *Error);
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleQueryStoreOffersCompleted] Logged %d offer(s)."), OfferIds.Num());

    // OSS caches the full FOnlineStoreOffer; GetOffer(id) returns it without re-querying.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineStoreV2Ptr Store = Subsystem ? Subsystem->GetStoreV2Interface() : nullptr;
    if (!Store.IsValid()) { return; }

    for (const FUniqueOfferId& OfferId : OfferIds)
    {
        TSharedPtr<FOnlineStoreOffer> Offer = Store->GetOffer(OfferId);
        if (Offer.IsValid())
        {
            UE_LOG(LogEOSOSSTutorial, VeryVerbose, TEXT("[AEOSPlayerController::HandleQueryStoreOffersCompleted] OfferId=%s Title=\"%s\" Price=%s"),
                *OfferId, *Offer->Title.ToString(), *Offer->PriceText.ToString());
        }
    }
}

void AEOSPlayerController::QueryStoreReceipts(bool bRestoreReceipts)
{
    // bRestoreReceipts=true forces a backend re-fetch instead of OSS's cached
    // list (typically used after reinstall when local cache is empty).
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;
    if (!Identity.IsValid() || !Purchase.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryStoreReceipts] Identity or Purchase interface null."));
        return;
    }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::Type::LoggedIn)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryStoreReceipts] Player not logged in."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::QueryStoreReceipts] Querying receipts (bRestoreReceipts=%s)..."),
        bRestoreReceipts ? TEXT("true") : TEXT("false"));

    Purchase->QueryReceipts(*LocalNetId, bRestoreReceipts,
        FOnQueryReceiptsComplete::CreateUObject(this, &ThisClass::HandleQueryStoreReceiptsCompleted));
}

void AEOSPlayerController::HandleQueryStoreReceiptsCompleted(const FOnlineError& Error)
{
    if (!Error.bSucceeded)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleQueryStoreReceiptsCompleted] Failed: %s"), *Error.ToLogString());
        return;
    }

    // Re-fetch the local user since QueryReceipts' callback signature only
    // surfaces the FOnlineError, not the user it was called for.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;
    if (!Identity.IsValid() || !Purchase.IsValid()) { return; }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid()) { return; }

    TArray<FPurchaseReceipt> Receipts;
    Purchase->GetReceipts(*LocalNetId, Receipts);
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleQueryStoreReceiptsCompleted] Logged %d receipt(s)."), Receipts.Num());

    // OSS-EOS quirks for the QueryReceipts path. Three fields stay empty
    // on this code path even though they exist on the structs:
    //   - OfferEntry.OfferId    : OSS hardcodes "" (Engine/.../OnlinePurchaseEOS.cpp:127).
    //   - Receipt.TransactionId : QueryReceipts uses EOS_Ecom_QueryOwnershipBySandboxIds
    //                              which returns ownership state, not a transaction.
    //   - LineItem.ItemName     : QueryReceipts skips EOS_Ecom_CopyItemById
    //                              (only the Checkout path calls it).
    // Populated data: LineItems[0].UniqueId = EOS_Ecom_CatalogItemId.
    //
    // QueryReceipts vs QueryEntitlements:
    //   QueryReceipts     -> durable / non-consumable content (DLC, expansions).
    //   QueryEntitlements -> consumable content + consumption state
    //                        (ConsumedCount tracks redemption).
    // For Name / bIsConsumable / ConsumedCount / EndDate, see
    // HandleQueryEntitlementsCompleted (wraps EOS_Ecom_CopyEntitlementByIndex).
    for (const FPurchaseReceipt& Receipt : Receipts)
    {
        for (const FPurchaseReceipt::FReceiptOfferEntry& OfferEntry : Receipt.ReceiptOffers)
        {
            if (OfferEntry.LineItems.IsEmpty()) { continue; }
            const FPurchaseReceipt::FLineItemInfo& LineItem = OfferEntry.LineItems[0];
            UE_LOG(LogEOSOSSTutorial, VeryVerbose, TEXT("[AEOSPlayerController::HandleQueryStoreReceiptsCompleted] TransactionId=%s OwnedItemId=%s ItemName=\"%s\""),
                *Receipt.TransactionId, *LineItem.UniqueId, *LineItem.ItemName);
        }
    }
}

void AEOSPlayerController::CheckoutOffer(const FString& OfferId, int32 Quantity, bool bConsumable)
{
    // Tutorial 3: Open the EGS wallet overlay so the player can buy this offer.
    // This call returns immediately; the result fires asynchronously when the
    // player closes the overlay (success on confirm, failure on cancel /
    // network error). REQUIRES a packaged Win64 client - the EOS overlay is not
    // available in PIE.
    if (OfferId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CheckoutOffer] Empty OfferId."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;
    if (!Identity.IsValid() || !Purchase.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CheckoutOffer] Identity or Purchase interface null."));
        return;
    }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::Type::LoggedIn)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CheckoutOffer] Player not logged in."));
        return;
    }

    FPurchaseCheckoutRequest Request = {};
    // EOS doesn't use OfferNamespace - empty string is correct.
    Request.AddPurchaseOffer(TEXT(""), OfferId, Quantity, bConsumable);

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::CheckoutOffer] Opening checkout overlay for OfferId=%s Quantity=%d bConsumable=%s..."),
        *OfferId, Quantity, bConsumable ? TEXT("true") : TEXT("false"));

    Purchase->Checkout(*LocalNetId, Request,
        FOnPurchaseCheckoutComplete::CreateUObject(this, &ThisClass::HandleCheckoutCompleted));
}

void AEOSPlayerController::HandleCheckoutCompleted(const FOnlineError& Error, const TSharedRef<FPurchaseReceipt>& Receipt)
{
    if (!Error.bSucceeded)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleCheckoutCompleted] Failed: %s"), *Error.ToLogString());
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleCheckoutCompleted] Success - TransactionId=%s, %d ReceiptOffers."),
        *Receipt->TransactionId, Receipt->ReceiptOffers.Num());

    // OSS-EOS quirk: FReceiptOfferEntry::OfferId is hardcoded "" (see
    // Engine/.../OnlinePurchaseEOS.cpp:127). The EOS Ecom SDK only surfaces
    // the resulting EOS_Ecom_Entitlement, not the source offer. Data lives
    // on LineItems[0]:
    //   UniqueId        = EOS_Ecom_CatalogItemId
    //   ValidationInfo  = EOS_Ecom_EntitlementId  (pass to RedeemEntitlement;
    //                                              empty if already redeemed)
    //   ItemName        = catalog item title text
    for (const FPurchaseReceipt::FReceiptOfferEntry& OfferEntry : Receipt->ReceiptOffers)
    {
        if (OfferEntry.LineItems.IsEmpty()) { continue; }
        const FPurchaseReceipt::FLineItemInfo& LineItem = OfferEntry.LineItems[0];
        UE_LOG(LogEOSOSSTutorial, VeryVerbose, TEXT("[AEOSPlayerController::HandleCheckoutCompleted] CatalogItemId=%s EntitlementId=%s ItemName=\"%s\""),
            *LineItem.UniqueId, *LineItem.ValidationInfo, *LineItem.ItemName);
    }

    // Re-query entitlements so the new purchase shows up in the cached list.
    QueryEntitlements();
}

void AEOSPlayerController::QueryEntitlements()
{
    // Tutorial 3: Fetch the player's entitlements (granted catalog items) for
    // this artifact. Unlike Store/Purchase which take a callback per call, the
    // entitlements interface uses the delegate-list pattern.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlineEntitlementsPtr Entitlements = Subsystem ? Subsystem->GetEntitlementsInterface() : nullptr;
    if (!Identity.IsValid() || !Entitlements.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryEntitlements] Identity or Entitlements interface null."));
        return;
    }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::Type::LoggedIn)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryEntitlements] Player not logged in."));
        return;
    }

    // Bind the completion callback before kicking off the query so we don't
    // miss a fast-fail. Namespace is empty - EOS doesn't use it.
    QueryEntitlementsDelegateHandle =
        Entitlements->AddOnQueryEntitlementsCompleteDelegate_Handle(
            FOnQueryEntitlementsCompleteDelegate::CreateUObject(this, &ThisClass::HandleQueryEntitlementsCompleted));

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::QueryEntitlements] Querying entitlements..."));
    if (!Entitlements->QueryEntitlements(*LocalNetId, TEXT("")))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryEntitlements] QueryEntitlements call failed."));
        Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
        QueryEntitlementsDelegateHandle.Reset();
    }
}

void AEOSPlayerController::HandleQueryEntitlementsCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Namespace, const FString& Error)
{
    // Namespace is unused on EOS - keep the signature for OSS conformance.
    (void)Namespace;

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineEntitlementsPtr Entitlements = Subsystem ? Subsystem->GetEntitlementsInterface() : nullptr;
    if (Entitlements.IsValid() && QueryEntitlementsDelegateHandle.IsValid())
    {
        Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
        QueryEntitlementsDelegateHandle.Reset();
    }

    if (!bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Failed: %s"), *Error);
        return;
    }

    if (!Entitlements.IsValid()) { return; }

    TArray<TSharedRef<FOnlineEntitlement>> AllEntitlements;
    Entitlements->GetAllEntitlements(UserId, TEXT(""), AllEntitlements);
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Logged %d entitlement(s)."), AllEntitlements.Num());

    for (const TSharedRef<FOnlineEntitlement>& Entitlement : AllEntitlements)
    {
        UE_LOG(LogEOSOSSTutorial, VeryVerbose, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Id=%s Name=%s ItemId=%s bIsConsumable=%s ConsumedCount=%d EndDate=%s"),
            *Entitlement->Id, *Entitlement->Name, *Entitlement->ItemId,
            Entitlement->bIsConsumable ? TEXT("true") : TEXT("false"),
            Entitlement->ConsumedCount, *Entitlement->EndDate);
    }
}

void AEOSPlayerController::RedeemEntitlement(const FString& EntitlementId)
{
    // Tutorial 3: Redeem (consume) a granted entitlement. Wraps
    // IOnlinePurchase::FinalizeReceiptValidationInfo. See header for the
    // safety story - production titles redeem on a trusted backend.
    if (EntitlementId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Empty EntitlementId."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;
    if (!Identity.IsValid() || !Purchase.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Identity or Purchase interface null."));
        return;
    }

    const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::Type::LoggedIn)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Player not logged in."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::RedeemEntitlement] Redeeming entitlement %s..."), *EntitlementId);

    // OSS signature is FinalizeReceiptValidationInfo(FString& InReceiptValidationInfo, ...) -
    // non-const ref because the call can mutate the validation info. Make a
    // local mutable copy so our public API can keep the idiomatic const-ref param.
    FString MutableEntitlementId = EntitlementId;
    Purchase->FinalizeReceiptValidationInfo(*LocalNetId, MutableEntitlementId,
        FOnFinalizeReceiptValidationInfoComplete::CreateWeakLambda(this,
            [](const FOnlineError& Error, const FString& ValidationInfo)
            {
                if (Error.bSucceeded)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::RedeemEntitlement] Success."));
                }
                else
                {
                    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Failed: %s"), *Error.ToLogString());
                }
            }));
}

void AEOSPlayerController::TestCheckoutOffer(const FString& OfferId)
{
    // Console wrapper around CheckoutOffer. Already-owned items ($0 base game)
    // and items still pending backend processing will fail the checkout call.
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::TestCheckoutOffer] Console-triggered checkout for OfferId=%s"), *OfferId);
    CheckoutOffer(OfferId);
}

void AEOSPlayerController::TestRedeemEntitlement(const FString& EntitlementId)
{
    // Console wrapper around RedeemEntitlement. Already-redeemed entitlements
    // (ConsumedCount > 0) won't re-redeem.
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::TestRedeemEntitlement] Console-triggered redeem for EntitlementId=%s"), *EntitlementId);
    RedeemEntitlement(EntitlementId);
}

// =====================================================================
// Tutorial 4: This code will only be included if P2PMode is enabled.

#if P2PMODE

// Empty stub - unused in P2P (lobby-managed voice), but UHT requires the symbol.
void AEOSPlayerController::Client_ReceiveVoiceCredentials_Implementation(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl)
{
}

// Tutorial 10: stubs for the dedicated-server AC RPCs (unused in P2P listen-server mode).
// Server_NotifyAntiCheatReady_Implementation is now cross-mode (see above).
void AEOSPlayerController::Server_AntiCheatMessage_Implementation(const TArray<uint8>& Bytes)
{
}

void AEOSPlayerController::Client_AntiCheatMessage_Implementation(const TArray<uint8>& Bytes)
{
}

// Tutorial 10 (P2P branch): non-host's violation report - host logs for telemetry only.
// Host's own SDK fires independently and does the kick; this is not authoritative.
void AEOSPlayerController::Server_PeerViolationDetected_Implementation(const FUniqueNetIdRepl& OffendingPlayer, const FString& Reason)
{
#if ACMODE
    FUniqueNetIdPtr OffenderId = OffendingPlayer.GetUniqueNetId();
    const FString OffenderStr = OffenderId.IsValid() ? OffenderId->ToString() : FString(TEXT("<invalid>"));
    UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::Server_PeerViolationDetected] Non-host peer reported violation on %s: %s"),
        *OffenderStr, *Reason);
#endif
}

// =====================================================================
// Tutorial 4: Lobbies (P2PMODE=1 only - listen-server-over-P2P).
//
// IOnlineSession surfaced as a lobby via bUseLobbiesIfAvailable=true.
// The lobby host calls Listen() to become the UE listen-server; other
// peers ClientTravel to the host's PUID via NetDriverEOS. Lobby
// participant + settings notifications drive runtime state (peer
// register/unregister for Tutorial 10 AC, voice channel auto-attach
// for the OSS-managed lobby voice in Tutorial 11). See Tutorial 5 for
// the find/join half (shared call sites) and Tutorial 10's Followups.md
// item for the JWT-verify-on-host trust model.
// =====================================================================

void AEOSPlayerController::CreateLobby(FName KeyName, FString KeyValue)
{
    // Tutorial 4: Create lobby - this code is similar to creating session, notice that bIsDedicated is false, bUseLobbiesIfAvailable and UseLobbiesVoiceChatIfAvailable is true.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        CreateLobbyDelegateHandle =
            Session->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleCreateLobbyCompleted));

        TSharedRef<FOnlineSessionSettings> SessionSettings = MakeShared<FOnlineSessionSettings>();
        SessionSettings->NumPublicConnections = 3; // Host + 2 non-hosts for the Tutorial 10 P2P kick-telemetry test.
        SessionSettings->bShouldAdvertise = true; // This creates a public match and will be searchable.
        // Tutorial 4: P2P lobbies are presence-native and have a local user
        // (the host). Enable presence + invites so the EOS Social Overlay
        // reflects "in lobby" and friends can be invited via overlay or Exec.
        SessionSettings->bUsesPresence = true;
        SessionSettings->bAllowJoinViaPresence = true;
        SessionSettings->bAllowJoinViaPresenceFriendsOnly = false; // any friend can join via overlay
        SessionSettings->bAllowInvites = true;
        SessionSettings->bAllowJoinInProgress = false; // Once the session is started, no one can join.
        SessionSettings->bIsDedicated = false; // P2P lobby, not a dedicated-server session.
        SessionSettings->bUseLobbiesIfAvailable = true; // For P2P we will use a lobby instead of a session.
        SessionSettings->bUseLobbiesVoiceChatIfAvailable = true; // We will also enable voice.
        SessionSettings->bUsesStats = true; // Needed to keep track of player stats.
        SessionSettings->Settings.Add(KeyName, FOnlineSessionSetting((KeyValue), EOnlineDataAdvertisementType::ViaOnlineService));

        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::CreateLobby] Creating lobby..."));

        if (!Session->CreateSession(0, LobbyName, *SessionSettings))
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CreateLobby] CreateSession call failed."));
            Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyDelegateHandle);
            CreateLobbyDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CreateLobby] Session interface null"));
    }
}

void AEOSPlayerController::HandleCreateLobbyCompleted(FName EOSLobbyName, bool bWasSuccessful)
{
    // Tutorial 4: Callback function - this is called once our lobby is created.
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleCreateLobbyCompleted] Lobby %s created."), *EOSLobbyName.ToString());
        FString Map = "Game/Content/ThirdPerson/Maps/ThirdPersonMap?listen"; // Hardcoding map name here, should be passed by parameter.
        FURL TravelURL;
        TravelURL.Map = Map;
        GetWorld()->Listen(TravelURL);
        SetupNotifications(); // Setup our listeners for lobby notification events.

        // Tutorial 11: lobby is up and bUseLobbiesVoiceChatIfAvailable=true caused the OSS-EOS
        // plugin to set up a voice user keyed by our LocalNetId. Bind the speaking-state
        // notification on it. Idempotent - safe to no-op if voice isn't ready yet.
        BindVoiceChatPlayerTalkingNotification();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleCreateLobbyCompleted] Failed to create lobby."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyDelegateHandle);
        CreateLobbyDelegateHandle.Reset();
    }
}

void AEOSPlayerController::SetupNotifications()
{
    // Tutorial 4: EOS Lobbies are great as there are notifications sent from our backend when there are changes to lobbies (ex: Participant Joins/Leaves, lobby or lobby member data is updated, etc...).
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        // In this tutorial we're only giving an example of a notification for when a participant joins/leaves the lobby. The approach is similar for other notifications.
        // Note: As of UE 5.2 the legacy FOnSessionParticipantsChangeDelegate was split into two separate delegates
        // (joined / left) and the leave event now carries an EOnSessionParticipantLeftReason.
        // We bind both here and keep the returned handles as members so EndPlay can clear them - otherwise
        // the OSS multicast lists would retain stale bindings after this controller is destroyed.
        ParticipantJoinedDelegateHandle =
            Session->AddOnSessionParticipantJoinedDelegate_Handle(FOnSessionParticipantJoinedDelegate::CreateUObject(
                this,
                &ThisClass::HandleParticipantJoined));

        ParticipantLeftDelegateHandle =
            Session->AddOnSessionParticipantLeftDelegate_Handle(FOnSessionParticipantLeftDelegate::CreateUObject(
                this,
                &ThisClass::HandleParticipantLeft));

#if ACMODE
        // Tutorial 10 (P2P branch): catches participants already in the lobby on join.
        SessionSettingsUpdatedDelegateHandle =
            Session->AddOnSessionSettingsUpdatedDelegate_Handle(FOnSessionSettingsUpdatedDelegate::CreateUObject(
                this,
                &ThisClass::HandleSessionSettingsUpdated));

        // Tutorial 10 (P2P branch): catches later-arrivers the OSS routes through
        // settings-updated instead of participant-joined on the joiner side.
        ParticipantSettingsUpdatedDelegateHandle =
            Session->AddOnSessionParticipantSettingsUpdatedDelegate_Handle(FOnSessionParticipantSettingsUpdatedDelegate::CreateUObject(
                this,
                &ThisClass::HandleParticipantSettingsUpdated));
#endif
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::SetupNotifications] Session interface null"));
    }
}

void AEOSPlayerController::HandleSessionSettingsUpdated(FName UpdatedSessionName, const FOnlineSessionSettings& UpdatedSettings)
{
#if ACMODE
    // OSS routing quirk (independent of EAC SDK notify-binding timing): when a
    // joiner enters a populated lobby, FOnlineSessionEOS::UpdateOrAddLobbyMember
    // routes already-present members through OnSessionSettingsUpdated instead of
    // OnSessionParticipantJoined (the !bWasLobbyMemberAdded path). Without this
    // walk, Client3 joining a 2-player lobby misses the existing peers and they
    // spurious-kick at the auth-timeout. RegisterPeer dedupes.
    if (UpdatedSessionName != LobbyName || UpdatedSettings.MemberSettings.Num() <= 1)
    {
        return;
    }

    // Tutorial 10 (P2P branch): the listen-server host doesn't RegisterPeer joiners
    // here - it defers to the verify-success path in Server_NotifyAntiCheatReady.
    // Other peers continue to RegisterPeer via the lobby (no JWT verify, lobby PUID
    // trust - acceptable since they're not the authority).
    if (HasAuthority())
    {
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* AntiCheatClient = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!LocalNetId.IsValid() || !AntiCheatClient)
    {
        return;
    }

    int32 NonLocalCount = 0;
    for (const TPair<FUniqueNetIdRef, FSessionSettings>& MemberPair : UpdatedSettings.MemberSettings)
    {
        if (*MemberPair.Key != *LocalNetId)
        {
            AntiCheatClient->RegisterPeer(MemberPair.Key);
            ++NonLocalCount;
        }
    }
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleSessionSettingsUpdated] Walked %d non-local participant(s); RegisterPeer dedupes already-registered peers."),
        NonLocalCount);
#endif // ACMODE
}

void AEOSPlayerController::HandleParticipantSettingsUpdated(FName UpdatedSessionName, const FUniqueNetId& ParticipantId, const FOnlineSessionSettings& /*Settings*/)
{
#if ACMODE
    if (UpdatedSessionName != LobbyName)
    {
        return;
    }

    // Tutorial 10 (P2P branch): host defers RegisterPeer to the verify-success path.
    if (HasAuthority())
    {
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* AntiCheatClient = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!LocalNetId.IsValid() || !AntiCheatClient || ParticipantId == *LocalNetId)
    {
        return;
    }

    // RegisterPeer dedupes - also fires for member-attribute changes (presence, voice).
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleParticipantSettingsUpdated] Participant %s settings update - registering with EAC if new."),
        *ParticipantId.ToString());
    AntiCheatClient->RegisterPeer(ParticipantId.AsShared());
#endif
}

void AEOSPlayerController::HandleParticipantJoined(FName EOSLobbyName, const FUniqueNetId& NetId)
{
    // Tutorial 4: Callback function called when a participant joins. Log the lobby name reported by the
    // callback rather than the local hardcoded member, so the message reflects what the OSS actually fired.
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleParticipantJoined] Player joined lobby %s."), *EOSLobbyName.ToString());

#if ACMODE
    // Tutorial 10 (P2P branch): host defers RegisterPeer to the verify-success path
    // (Server_NotifyAntiCheatReady). Non-host peers RegisterPeer here directly with
    // lobby PUID trust - acceptable because non-hosts aren't the authority.
    if (HasAuthority())
    {
        return;
    }

    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!Client) { return; }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (LocalNetId.IsValid() && *LocalNetId == NetId)
    {
        return;
    }
    Client->RegisterPeer(NetId.AsShared());
#endif
}

void AEOSPlayerController::HandleParticipantLeft(FName EOSLobbyName, const FUniqueNetId& NetId, EOnSessionParticipantLeftReason LeaveReason)
{
    // Tutorial 4: Callback function called when a participant leaves. LeaveReason tells us why (Left / Disconnected / Kicked / Closed).
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleParticipantLeft] Player left lobby %s."), *EOSLobbyName.ToString());

#if ACMODE
    // Tutorial 10 (P2P branch): UnregisterPeer so EAC stops expecting heartbeats.
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (Client)
    {
        Client->UnregisterPeer(NetId.AsShared());
    }
#endif
}

#if ACMODE
void AEOSPlayerController::StartAntiCheatPeerSession()
{
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!Client)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::StartAntiCheatPeerSession] AntiCheat client interface unavailable."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!LocalNetId.IsValid())
    {
        // Caller is responsible for sequencing this after EOS Connect login.
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::StartAntiCheatPeerSession] No local net id - login should have completed before calling this."));
        return;
    }

    Client->BeginSession(IEOSAntiCheatClient::EMode::PeerToPeer, LocalNetId.ToSharedRef());
}

void AEOSPlayerController::BindAntiCheatViolationDelegate()
{
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    if (!AntiCheat)
    {
        return;
    }

    // Idempotent: if a stale handle is still on this PC (shouldn't happen,
    // but defensive), drop it before re-binding to avoid leaking a slot.
    if (AntiCheatViolationDelegateHandle.IsValid())
    {
        AntiCheat->OnViolation().Remove(AntiCheatViolationDelegateHandle);
    }
    AntiCheatViolationDelegateHandle = AntiCheat->OnViolation().AddUObject(this, &ThisClass::HandleAntiCheatViolationP2P);
}

void AEOSPlayerController::UnbindAntiCheatViolationDelegate()
{
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    if (AntiCheat && AntiCheatViolationDelegateHandle.IsValid())
    {
        AntiCheat->OnViolation().Remove(AntiCheatViolationDelegateHandle);
        AntiCheatViolationDelegateHandle.Reset();
    }
    // Plugin shutdown calls Client->EndSession(); matches the server-mode lifetime model.
}

void AEOSPlayerController::HandleAntiCheatViolationP2P(const FUniqueNetIdPtr& OffendingPlayer, const FString& Reason)
{
    // PEER_SELF: local SDK flagged us. Log + exit; other peers' SDKs drop us independently.
    if (!OffendingPlayer.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleAntiCheatViolationP2P] Local client flagged: %s - exiting."), *Reason);
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick([]
            {
                FGenericPlatformMisc::RequestExit(false);
            });
        }
        return;
    }

    const ENetMode Mode = GetNetMode();
    if (Mode == NM_ListenServer)
    {
        // Lobby host has kick authority; offender's own SDK exits via PEER_SELF.
        AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
        AGameSession* GS = GM ? GM->GameSession : nullptr;
        if (!GS) { return; }

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && PC->PlayerState && PC->PlayerState->GetUniqueId() == *OffendingPlayer)
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleAntiCheatViolationP2P] Host kicking %s: %s"), *PC->GetName(), *Reason);
                GS->KickPlayer(PC, FText::FromString(Reason));
                return;
            }
        }
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::HandleAntiCheatViolationP2P] Host: no PC for %s - offender may have already left."),
            *OffendingPlayer->ToString());
        return;
    }

    // Non-host: no kick authority; relay to host for telemetry. Host's SDK does the drop.
    UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::HandleAntiCheatViolationP2P] Peer violation observed on %s: %s"),
        *OffendingPlayer->ToString(), *Reason);
    Server_PeerViolationDetected(FUniqueNetIdRepl(OffendingPlayer), Reason);
}
#endif // ACMODE
#endif // P2PMODE
// Tutorial 5: Sessions - client-side find/join.
//
// IOnlineSession::FindSessions + JoinSession against the dedicated-server
// session created in EOSGameSession.cpp (also Tutorial 5, server side).
// The P2PMODE=1 build reuses the same call sites against the lobby
// surfaced as a session (bUseLobbiesIfAvailable=true) - see Tutorial 4
// for the lobby creation half. Invite/accept paths are tracked here as
// well (HandleSessionUserInviteAccepted, HandleSessionInviteReceived,
// TestSendSessionInvite, TestAcceptLastInvite).
// =====================================================================

void AEOSPlayerController::FindSessions(FName SearchKey, FString SearchValue) // put default value for example
{
    // Tutorial 5: This function will find our EOS Session that was created by our DedicatedServer.
    // Tutorial 4: This function will find our EOS lobby. Note that at the OSS layer we are using a Session that is marked as a lobby. Code is similar with minor tweaks.
    // Tutorial 9: Refuse to search if the local player is sanctioned.
    // A real game would surface this in UI and offer the appeal flow.
    if (bRestrictMatchmaking)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::FindSessions] Local player is restricted from online play - skipping session search."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();

        // Remove the default search parameters that FOnlineSessionSearch sets up.
        Search->QuerySettings.SearchParams.Empty();

        Search->QuerySettings.Set(SearchKey, SearchValue, EOnlineComparisonOp::Equals); // Search using our Key/Value pair.
#if P2PMODE
        Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
#endif
        FindSessionsDelegateHandle =
            Session->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleFindSessionsCompleted,
                Search));
#if P2PMODE
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::FindSessions] Finding lobby."));
#else
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::FindSessions] Finding session."));
#endif

        if (!Session->FindSessions(0, Search))
        {
#if P2PMODE
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::FindSessions] Find lobby call failed."));
#else
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::FindSessions] Find session call failed."));
#endif
            Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
            FindSessionsDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::FindSessions] Session interface null"));
    }
}

void AEOSPlayerController::HandleFindSessionsCompleted(bool bWasSuccessful, TSharedRef<FOnlineSessionSearch> Search)
{
    // Tutorial 5: This function is triggered via the callback we set in FindSession once the session is found (or there is a failure).
    // Tutorial 4: This function is triggered via the callback we set in FindSession once the lobby is found (or there is a failure). Finding the lobby here has similar code to finding a session.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        if (bWasSuccessful)
        {
            // Added code here to not run into issues when searching for sessions is successful but the number of sessions is 0.
            if (Search->SearchResults.Num() == 0)
            {
#if P2PMODE
                // If we're in P2P mode and we can't find a lobby on startup, create one.
                CreateLobby();
#endif
                Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
                FindSessionsDelegateHandle.Reset();
                return;
            }
#if P2PMODE
            UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleFindSessionsCompleted] Found lobby."));
#else
            UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleFindSessionsCompleted] Found session."));
#endif
            for (auto SessionInSearchResult : Search->SearchResults)
            {
                // Typically you want to check if the session is valid before joining. There is a bug in the EOS OSS where IsValid() returns false when the session is created on a DS.
                // Instead of customizing the engine for this tutorial, we're simply not checking if the session is valid. The code below should go in this if statement once the bug is fixed.
                /*
                if (SessionInSearchResult.IsValid())
                {
                }
                */

                // Ensure the connection string is resolvable and store a *copy* of the result in SessionToJoin.
                // Must be a value copy: the TSharedRef<FOnlineSessionSearch> that owns SearchResults is dropped
                // when this callback returns, after which any pointer into its array would dangle.
                if (Session->GetResolvedConnectString(SessionInSearchResult, NAME_GamePort, ConnectString))
                {
                    SessionToJoin = SessionInSearchResult;
                }

                // For this course we will join the first session found automatically. Usually you would loop through all the sessions and determine which one is best to join.
                break;
            }
            JoinSession();
        }
        else
        {
#if P2PMODE
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleFindSessionsCompleted] Find lobby failed."));
#else
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleFindSessionsCompleted] Find session failed."));
#endif
        }

        Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
        FindSessionsDelegateHandle.Reset();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleFindSessionsCompleted] Session interface null"));
    }
}

void AEOSPlayerController::JoinSession()
{
    // Tutorial 5: Join the session.
    // Tutorial 4: Same code is used to join the lobby - just some tweaks to the logging.
    if (!SessionToJoin.IsSet())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::JoinSession] No session selected - HandleFindSessionsCompleted did not populate SessionToJoin."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        JoinSessionDelegateHandle =
            Session->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleJoinSessionCompleted));

        // Local cache key the client uses to refer to the joined session. Match the host's name so
        // future Session->Get/End/Destroy calls from this client line up.
#if P2PMODE
        const FName LocalSessionName = LobbyName;
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::JoinSession] Joining lobby."));
#else
        const FName LocalSessionName = TEXT("SessionName");
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::JoinSession] Joining session."));
#endif

        // Tutorial 5: Mark this join as the local user's presence session so
        // the EOS Social Overlay attaches to it (surfaces "in session" + the
        // Invite-to-Game button). Server-mode sessions are created with
        // bUsesPresence=false (server has no local user; SDK rejects
        // bPresenceEnabled=true at create). The game-side fix per
        // OnlineSessionEOS.cpp:4890 is to set bUsesPresence=true on the
        // search result before JoinSession - the OSS reads it from
        // SessionSettings and passes bPresenceEnabled=true to the SDK,
        // which the SDK accepts because the *joiner* has a local user.
        // P2P lobbies already create with these flags true, so this is a
        // no-op there.
        SessionToJoin->Session.SessionSettings.bUsesPresence = true;
        SessionToJoin->Session.SessionSettings.bAllowJoinViaPresence = true;

        if (!Session->JoinSession(0, LocalSessionName, SessionToJoin.GetValue()))
        {
#if P2PMODE
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::JoinSession] Join lobby call failed."));
#else
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::JoinSession] Join session call failed."));
#endif
            Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
            JoinSessionDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::JoinSession] Session interface null"));
    }
}

void AEOSPlayerController::HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    // Tutorial 5: This function is triggered via the callback we set in JoinSession once the session is joined (or there is a failure).
#if P2PMODE
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleJoinSessionCompleted] Joined lobby."));
        // Bind lobby notifications BEFORE ClientTravel - travel tears down this controller on the
        // client, so binding after the travel request would attach a dying `this` to the session.
        SetupNotifications();

        // Tutorial 10 (P2P branch): existing participants land via OnSessionSettingsUpdated
        // (CopyLobbyData populates MemberSettings async after this completes).
        ClientTravel(ConnectString, TRAVEL_Absolute);
    }
#else
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleJoinSessionCompleted] Joined session."));
        UWorld* World = GetWorld();
        if (GEngine && World)
        {
            // For the purposes of this tutorial we override the ConnectString to point to localhost as we are testing locally. In a real game no need to override. Make sure you can connect over UDP to the ip:port of your server!
            ConnectString = "127.0.0.1:7777";
            FURL DedicatedServerURL(nullptr, *ConnectString, TRAVEL_Absolute);
            FString DedicatedServerJoinError;
            FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World); // Non-Checked variant - returns nullptr instead of asserting if no context.
            if (WorldContext)
            {
                auto DedicatedServerJoinStatus = GEngine->Browse(*WorldContext, DedicatedServerURL, DedicatedServerJoinError);
                if (DedicatedServerJoinStatus == EBrowseReturnVal::Failure)
                {
                    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleJoinSessionCompleted] Failed to browse for dedicated server: %s"), *DedicatedServerJoinError);
                }
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleJoinSessionCompleted] No WorldContext for the current world - cannot browse to the dedicated server."));
            }

            // To be thorough here you should modify your derived UGameInstance to handle the NetworkError and TravelError events.
            // This should also be done to handle the error "FULL" returned by the server.
            // As we are testing locally, and for the purposes of keeping this tutorial simple, this is omitted.
        }

        // Tutorial 11: Voice startup is server-driven - see Client_ReceiveVoiceCredentials below.
    }
#endif

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
        JoinSessionDelegateHandle.Reset();
    }
}

// =====================================================================
// Tutorial 6: EOS Social - Presence + Friends + Overlay.
// Client-only (all three interfaces require a local user). Wraps three
// generic OSS interfaces, all EOS-backed by OSS-EOS plugin's UserManagerEOS:
//   - IOnlinePresence: publish + query rich-text status (custom
//     game-defined text the EOS Social Overlay surfaces on friend rows).
//   - IOnlineFriends: read the local user's friends list (display
//     names, presence summary), react to friend status changes via
//     OnFriendsChange. FOnlineFriend::GetDisplayName + GetPresence
//     give us per-friend identity + status without a separate UserInfo
//     query (IOnlineUser::GetUserInfo is broken for remote users in
//     OSS-EOS UE 5.8 - only iterates local users).
//   - IOnlineExternalUI: programmatically open the EOS Social Overlay
//     Friends list. Profile + Invite views can't be opened
//     programmatically - the EOS UI SDK only exposes ShowFriends,
//     ShowBlockPlayer, and ShowReportPlayer; the latter two aren't
//     surfaced through the OSS layer at all. Profile/invite are
//     reachable only via user navigation inside the overlay itself.
// Sits above the session-attached presence (Tutorial 4/5's bUsesPresence
// join-time flip), which only declares "I'm in session X" - this layer
// adds the custom status string + the social-graph surface around it.
// =====================================================================

void AEOSPlayerController::SetGamePresence(const FString& StatusText)
{
    // Tutorial 6: Publish status + properties for the local user. The
    // EOS Social Overlay reads StatusStr verbatim. Properties are a
    // game-defined key/value map (max 32 keys, key <= 64 chars,
    // value <= 255 chars per the EOS SDK's EOS_PRESENCE_DATA_* limits).
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlinePresencePtr Presence = Subsystem ? Subsystem->GetPresenceInterface() : nullptr;
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!Presence.IsValid() || !LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::SetGamePresence] Presence/Identity unavailable."));
        return;
    }

    FOnlineUserPresenceStatus Status;
    Status.State = EOnlinePresenceState::Online;
    Status.StatusStr = StatusText;
    // Example property - real games would set this to the current map,
    // game mode, character class, etc. Tutorial uses a static value to
    // demonstrate the mechanism without coupling to gameplay state.
    Status.Properties.Add(TEXT("Map"), FVariantData(FString(TEXT("ThirdPersonMap"))));

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::SetGamePresence] Setting status: \"%s\""),
        *StatusText);

    Presence->SetPresence(*LocalNetId, Status,
        IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateUObject(this, &ThisClass::HandleSetPresenceCompleted));
}

void AEOSPlayerController::HandleSetPresenceCompleted(const FUniqueNetId& UserId, bool bWasSuccessful)
{
    // Tutorial 6: SetPresence completion. Async; can fail if the SDK
    // rejects the status (e.g. exceeds rich-text length limits).
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[AEOSPlayerController::HandleSetPresenceCompleted] Presence updated for %s."),
            *UserId.ToDebugString());
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleSetPresenceCompleted] SetPresence failed for %s."),
            *UserId.ToDebugString());
    }
}

void AEOSPlayerController::QueryPresenceFor(const FUniqueNetIdRef& Target)
{
    // Tutorial 6: Async query of a remote user's presence. Result lands
    // in HandleQueryPresenceCompleted (one-shot) and the multicast
    // OnPresenceReceived (HandlePresenceReceived). The cache is
    // populated either way, so subsequent GetCachedPresence reads are
    // sync.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlinePresencePtr Presence = Subsystem ? Subsystem->GetPresenceInterface() : nullptr;
    if (!Presence.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::QueryPresenceFor] Presence interface null."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::QueryPresenceFor] Querying %s."),
        *Target->ToDebugString());

    Presence->QueryPresence(*Target,
        IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateUObject(this, &ThisClass::HandleQueryPresenceCompleted));
}

void AEOSPlayerController::TestQueryPresence(const FString& TargetProductUserId)
{
    // Tutorial 6: Console wrapper - resolves a raw PUID to an
    // FUniqueNetIdRef and forwards to QueryPresenceFor.
    if (TargetProductUserId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestQueryPresence] ProductUserId required - copy it from the other client's Login succeeded log line."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    if (!Identity.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestQueryPresence] Identity interface null."));
        return;
    }

    // Same shape as TestSendSessionInvite: prefix with "|" so OSS-EOS
    // CreateUniquePlayerId parses as PUID-only.
    const FString NetIdString = FString::Printf(TEXT("|%s"), *TargetProductUserId);
    FUniqueNetIdPtr Target = Identity->CreateUniquePlayerId(NetIdString);
    if (!Target.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestQueryPresence] CreateUniquePlayerId failed for '%s'."),
            *TargetProductUserId);
        return;
    }

    QueryPresenceFor(Target.ToSharedRef());
}

void AEOSPlayerController::HandleQueryPresenceCompleted(const FUniqueNetId& UserId, bool bWasSuccessful)
{
    // Tutorial 6: Query completion. On success, GetCachedPresence reads
    // the populated cache. The same data also lands via
    // OnPresenceReceived (HandlePresenceReceived) below.
    if (!bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleQueryPresenceCompleted] QueryPresence failed for %s."),
            *UserId.ToDebugString());
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlinePresencePtr Presence = Subsystem ? Subsystem->GetPresenceInterface() : nullptr;
    if (!Presence.IsValid())
    {
        return;
    }

    TSharedPtr<FOnlineUserPresence> CachedPresence;
    if (Presence->GetCachedPresence(UserId, CachedPresence) == EOnlineCachedResult::Success && CachedPresence.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[AEOSPlayerController::HandleQueryPresenceCompleted] %s -> %s"),
            *UserId.ToDebugString(), *CachedPresence->ToDebugString());
    }
}

void AEOSPlayerController::HandlePresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
{
    // Tutorial 6: Multicast - fires for any user whose presence
    // changed. Logged for visibility; real games would refresh UI bound
    // to that user (friends list status, in-session player rows, etc.).
    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::HandlePresenceReceived] %s -> %s"),
        *UserId.ToDebugString(), *Presence->ToDebugString());
}

void AEOSPlayerController::TestSetGamePresence(const FString& StatusText)
{
    // Tutorial 6: Console wrapper for SetGamePresence. Lets the demo
    // show exactly when presence changes by tying it to a manual
    // trigger - no auto-fire on login or join, so the EOS Social Overlay
    // surfaces the rich text the user explicitly set without races
    // against rapid back-to-back presence updates.
    if (IsRunningDedicatedServer())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetGamePresence] Client-only API; ignored on server."));
        return;
    }
    if (StatusText.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSetGamePresence] StatusText required."));
        return;
    }

    SetGamePresence(StatusText);
}

void AEOSPlayerController::ReadFriendsList()
{
    // Tutorial 6: Async fetch of the local user's friends list. The
    // "default" filter returns the player's full Epic friends list.
    // Other filters (OnlinePlayers, InGamePlayers, InGameAndSessionPlayers)
    // are also valid - see EFriendsLists in OnlineFriendsInterface.h.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
    if (!Friends.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::ReadFriendsList] Friends interface null."));
        return;
    }

    const FString DefaultList = EFriendsLists::ToString(EFriendsLists::Default);
    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::ReadFriendsList] Reading list '%s'."),
        *DefaultList);

    if (!Friends->ReadFriendsList(0, DefaultList,
            FOnReadFriendsListComplete::CreateUObject(this, &ThisClass::HandleReadFriendsListCompleted)))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::ReadFriendsList] ReadFriendsList call failed."));
    }
}

void AEOSPlayerController::HandleReadFriendsListCompleted(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
    // Tutorial 6: ReadFriendsList completion. On success, sync-read the
    // populated cache via GetFriendsList and log each entry. The friend
    // entries also expose presence and name info via FOnlineFriend's
    // accessors (GetDisplayName, GetPresence) - useful for in-game UI.
    if (!bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleReadFriendsListCompleted] ReadFriendsList failed: %s"),
            *ErrorStr);
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
    if (!Friends.IsValid())
    {
        return;
    }

    TArray<TSharedRef<FOnlineFriend>> FriendsArray;
    if (!Friends->GetFriendsList(LocalUserNum, ListName, FriendsArray))
    {
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::HandleReadFriendsListCompleted] GetFriendsList returned false."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::HandleReadFriendsListCompleted] '%s' returned %d friend(s)."),
        *ListName, FriendsArray.Num());

    for (const TSharedRef<FOnlineFriend>& Friend : FriendsArray)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[Friend] %s (%s) - presence: %s"),
            *Friend->GetDisplayName(),
            *Friend->GetUserId()->ToDebugString(),
            *Friend->GetPresence().ToDebugString());
    }
}

void AEOSPlayerController::HandleFriendsChange()
{
    // Tutorial 6: Multicast - fires when the local user's friends list
    // membership changes (friend added, removed, accepted, etc.).
    // Logged for visibility; real games would refresh the in-game
    // friends list UI here.
    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::HandleFriendsChange] Friends list changed."));
}

void AEOSPlayerController::ShowFriendsOverlay()
{
    // Tutorial 6: Open the EOS Social Overlay to the Friends list view.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineExternalUIPtr External = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
    if (!External.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::ShowFriendsOverlay] ExternalUI interface null."));
        return;
    }

    if (!External->ShowFriendsUI(0))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::ShowFriendsOverlay] ShowFriendsUI call failed."));
    }
}

void AEOSPlayerController::TestReadFriends()
{
    // Tutorial 6: Console wrapper for ReadFriendsList.
    if (IsRunningDedicatedServer())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestReadFriends] Client-only API; ignored on server."));
        return;
    }
    ReadFriendsList();
}

void AEOSPlayerController::TestShowFriendsOverlay()
{
    // Tutorial 6: Console wrapper for ShowFriendsOverlay.
    if (IsRunningDedicatedServer())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestShowFriendsOverlay] Client-only API; ignored on server."));
        return;
    }
    ShowFriendsOverlay();
}
// =====================================================================
// Tutorial 7: Title Storage + Player Data Storage.
//
// Two OSS interfaces with parallel shapes:
//   - IOnlineTitleFile (IOnlineUserCloud's read-only sibling): server-
//     authored game data files (configs, level pack lists, etc). Read
//     by all players, never written.
//   - IOnlineUserCloud: per-player save data. Read + write under the
//     local user's PUID.
// Both APIs follow EnumerateFiles -> ReadFile -> consume-bytes. The
// player-cloud write path runs from AEOSPlayerController::SaveGame (on
// Esc) and writes the character location for restore on next login.
// Mode-agnostic.
// =====================================================================

void AEOSPlayerController::LoadTitleData()
{
    /*
    Tutorial 7: This function is triggered by the login callback once the player has logged in (client only).
    Files must first be enumerated before they can be read. In this course we will read from the 1st file.
    */
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem ? Subsystem->GetTitleFileInterface() : nullptr;

    if (TitleFile.IsValid())
    {
        EnumTitleFilesDelegateHandle = TitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(FOnEnumerateFilesCompleteDelegate::CreateUObject(
            this,
            &ThisClass::HandleEnumTitleFilesCompleted));

        if (!TitleFile->EnumerateFiles())
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::LoadTitleData] EnumerateFiles call failed."));
            TitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(EnumTitleFilesDelegateHandle);
            EnumTitleFilesDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::LoadTitleData] TitleFile interface null"));
    }
}

void AEOSPlayerController::HandleEnumTitleFilesCompleted(bool bWasSuccessfull, const FString& Error)
{
    // Tutorial 7: Callback function for enumerating title data storage files.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem ? Subsystem->GetTitleFileInterface() : nullptr;

    if (TitleFile.IsValid())
    {
        if (bWasSuccessfull)
        {
            // Set an array of files we can populate.
            TArray<FCloudFileHeader> TitleFiles;
            TitleFile->GetFileList(TitleFiles);

            // Get the filenames from our array of files.
            TArray<FString> TitleFileNames;
            for (const auto& File : TitleFiles)
            {
                TitleFileNames.Add(File.FileName);
            }

            ReadTitleFileDelegateHandle = TitleFile->AddOnReadFileCompleteDelegate_Handle(FOnReadFileCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleReadTitleFileCompleted));

            // To keep things simple we are only reading the 1st file which is a .txt file. We will output the file content to the logs.
            if (TitleFileNames.Num() > 0)
            {
                if (!TitleFile->ReadFile(TitleFileNames[0]))
                {
                    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumTitleFilesCompleted] ReadFile call failed for %s."), *TitleFileNames[0]);
                }
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::HandleEnumTitleFilesCompleted] No title storage files found."));
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumTitleFilesCompleted] EnumerateFiles failed: %s"), *Error);
        }

        TitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(EnumTitleFilesDelegateHandle);
        EnumTitleFilesDelegateHandle.Reset();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumTitleFilesCompleted] TitleFile interface null"));
    }
}

void AEOSPlayerController::HandleReadTitleFileCompleted(bool bWasSuccessfull, const FString& FileName)
{
    // Tutorial 7: Callback function for reading 1st file in title data storage.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineTitleFilePtr TitleFile = Subsystem ? Subsystem->GetTitleFileInterface() : nullptr;

    if (TitleFile.IsValid())
    {
        if (bWasSuccessfull)
        {
            TArray<uint8> FileContents; // We need an array to output the serialized title data storage file data.
            if (TitleFile->GetFileContents(FileName, FileContents))
            {
                // Copy into an owned, null-terminated ANSI buffer so we can interpret the bytes as a C-string.
                // Using TArray here (instead of raw new[]/delete) keeps allocation lifetime RAII-managed
                // and avoids the std::bad_alloc / delete[] pitfalls of the 5.1-era code.
                TArray<ANSICHAR> FileData;
                FileData.Append(reinterpret_cast<const ANSICHAR*>(FileContents.GetData()), FileContents.Num());
                FileData.Add('\0');

                const FString FileDataAsFString = ANSI_TO_TCHAR(FileData.GetData());
                // Check file contents and hardcode log outputs to prevent log injection.
                if (FileDataAsFString.Equals("Game data"))
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleReadTitleFileCompleted] File contents are: Game data"));
                }
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadTitleFileCompleted] GetFileContents failed for %s."), *FileName);
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadTitleFileCompleted] Error reading title storage file %s."), *FileName);
        }

        TitleFile->ClearOnReadFileCompleteDelegate_Handle(ReadTitleFileDelegateHandle);
        ReadTitleFileDelegateHandle.Reset();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadTitleFileCompleted] TitleFile interface null"));
    }
}

void AEOSPlayerController::WritePlayerDataStorage(FString FileName, TArray<uint8> FileData)
{
    // Tutorial 7: Function called to save game. This is called when the ESC key is pressed. See the Quit() function in the character class. Only called on Clients.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlineUserCloudPtr UserCloud = Subsystem ? Subsystem->GetUserCloudInterface() : nullptr;

    if (Identity.IsValid() && UserCloud.IsValid())
    {
        const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
        if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
        {
            WritePlayerDataStorageDelegateHandle =
                UserCloud->AddOnWriteUserFileCompleteDelegate_Handle(
                    FOnWriteUserFileCompleteDelegate::CreateUObject(
                        this,
                        &ThisClass::HandleWritePlayerDataStorageCompleted));

            if (!UserCloud->WriteUserFile(*LocalNetId, FileName, FileData))
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::WritePlayerDataStorage] WriteUserFile call failed for %s."), *FileName);
                UserCloud->ClearOnWriteUserFileCompleteDelegate_Handle(WritePlayerDataStorageDelegateHandle);
                WritePlayerDataStorageDelegateHandle.Reset();
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::WritePlayerDataStorage] Player not logged in - game will not be saved."));
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::WritePlayerDataStorage] Identity and/or UserCloud interface null"));
    }
}

void AEOSPlayerController::HandleWritePlayerDataStorageCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
    // Tutorial 7: Callback function when file write has completed.
    if (bWasSuccessful)
    {
        // If saving the game was a success, quit.
        ConsoleCommand(TEXT("quit"));
    }
    else
    {
        // This means that the game wasn't saved. The game should notify the player and not just quit.
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleWritePlayerDataStorageCompleted] Failed to write %s to player data storage."), *FileName);
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr UserCloud = Subsystem ? Subsystem->GetUserCloudInterface() : nullptr;

    if (UserCloud.IsValid())
    {
        UserCloud->ClearOnWriteUserFileCompleteDelegate_Handle(WritePlayerDataStorageDelegateHandle);
        WritePlayerDataStorageDelegateHandle.Reset();
    }
}

void AEOSPlayerController::LoadPlayerData()
{
    /*
    Tutorial 7: This function is triggered by the login callback once the player has logged in (client only).
    Files must first be enumerated before they can be read. In this course we will read from the 1st file only.
    */
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    IOnlineUserCloudPtr PlayerData = Subsystem ? Subsystem->GetUserCloudInterface() : nullptr;

    if (Identity.IsValid() && PlayerData.IsValid())
    {
        const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
        if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
        {
            EnumPlayerFilesDelegateHandle = PlayerData->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleEnumPlayerFilesCompleted));

            PlayerData->EnumerateUserFiles(*LocalNetId);
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::LoadPlayerData] Player not logged in - save game will not be loaded."));
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::LoadPlayerData] Identity and/or UserCloud interface null"));
    }
}

void AEOSPlayerController::HandleEnumPlayerFilesCompleted(bool bWasSuccessfull, const FUniqueNetId& NetId)
{
    // Tutorial 7: Callback function for enumerating player data storage files.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr PlayerData = Subsystem ? Subsystem->GetUserCloudInterface() : nullptr;

    if (PlayerData.IsValid())
    {
        if (bWasSuccessfull)
        {
            // Set an array of files we can populate.
            TArray<FCloudFileHeader> PlayerFiles;
            PlayerData->GetUserFileList(NetId, PlayerFiles);

            if (PlayerFiles.Num() == 0)
            {
                UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::HandleEnumPlayerFilesCompleted] No player files in player data storage."));
                PlayerData->ClearOnEnumerateUserFilesCompleteDelegate_Handle(EnumPlayerFilesDelegateHandle);
                EnumPlayerFilesDelegateHandle.Reset();
                return;
            }

            TArray<FString> PlayerFileNames;
            for (const auto& File : PlayerFiles)
            {
                PlayerFileNames.Add(File.FileName);
            }

            ReadPlayerDataFileDelegateHandle = PlayerData->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleReadPlayerFileCompleted));

            // To keep things simple we are only reading the 1st file. The player's initial location will change to the location of when the player quit the last instance.
            if (!PlayerData->ReadUserFile(NetId, PlayerFileNames[0]))
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumPlayerFilesCompleted] ReadUserFile call failed for %s."), *PlayerFileNames[0]);
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumPlayerFilesCompleted] EnumerateUserFiles failed."));
        }

        PlayerData->ClearOnEnumerateUserFilesCompleteDelegate_Handle(EnumPlayerFilesDelegateHandle);
        EnumPlayerFilesDelegateHandle.Reset();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleEnumPlayerFilesCompleted] UserCloud interface null"));
    }
}

void AEOSPlayerController::HandleReadPlayerFileCompleted(bool bWasSuccessfull, const FUniqueNetId& UserId, const FString& FileName)
{
    // Tutorial 7: Callback function for reading 1st file in player data storage.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineUserCloudPtr PlayerData = Subsystem ? Subsystem->GetUserCloudInterface() : nullptr;

    if (PlayerData.IsValid())
    {
        if (bWasSuccessfull)
        {
            TArray<uint8> FileContents;
            if (PlayerData->GetFileContents(UserId, FileName, FileContents))
            {
                // Should use a USaveGame.
                LoadGame(FileContents);
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadPlayerFileCompleted] GetFileContents failed for %s."), *FileName);
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadPlayerFileCompleted] Error reading player data storage file %s."), *FileName);
        }

        PlayerData->ClearOnReadUserFileCompleteDelegate_Handle(ReadPlayerDataFileDelegateHandle);
        ReadPlayerDataFileDelegateHandle.Reset();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleReadPlayerFileCompleted] UserCloud interface null"));
    }
}


// =====================================================================
// Tutorial 8: Player Reports - SendPlayerReport via the OSS-EOS plugin's
// public IOnlinePlayerReportEOS interface (no generic OSS abstraction).
// Mode-agnostic: same call works for server-mode sessions and P2P
// lobbies. Surfaced via Exec command - real games invoke this from a
// post-match report UI.
// =====================================================================

void AEOSPlayerController::SendPlayerReport(const FUniqueNetIdRef& Reporter, const FUniqueNetIdRef& Reported, const FString& Message, const FString& ContextJson)
{
    // OSS-EOS-only API: cast IOnlineSubsystem to IOnlineSubsystemEOS to reach
    // GetPlayerReportEOSInterface(). No generic IOnlinePlayerReports exists.
    IOnlineSubsystemEOS* const EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld()));
    IOnlinePlayerReportEOSPtr Reports = EOSSubsystem ? EOSSubsystem->GetPlayerReportEOSInterface() : nullptr;
    if (!Reports.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::SendPlayerReport] PlayerReport EOS interface null."));
        return;
    }

    IOnlinePlayerReportEOS::FSendPlayerReportSettings Settings;
    // Hardcoded category for the demo - real games surface a dropdown in the
    // post-match report UI bound to the full EPlayerReportCategory enum.
    Settings.Category = IOnlinePlayerReportEOS::EPlayerReportCategory::Cheating;
    Settings.Message  = Message;
    // SDK rejects the call if Context is non-empty and not valid JSON. Empty is allowed.
    Settings.Context  = ContextJson;

    Reports->SendPlayerReport(*Reporter, *Reported, MoveTemp(Settings),
        FOnSendPlayerReportComplete::CreateLambda(
            [ReportedShared = Reported](const bool bWasSuccessful)
            {
                if (bWasSuccessful)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose,
                        TEXT("[AEOSPlayerController::SendPlayerReport] Report submitted for %s."),
                        *ReportedShared->ToDebugString());
                }
                else
                {
                    UE_LOG(LogEOSOSSTutorial, Error,
                        TEXT("[AEOSPlayerController::SendPlayerReport] Report submission failed for %s."),
                        *ReportedShared->ToDebugString());
                }
            }));
}

void AEOSPlayerController::TestSendPlayerReport(const FString& Message, const FString& Context)
{
    // Console wrapper: pick the first non-self player in the local named
    // session as the report target. No-op if no session is active or no
    // other players are registered.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!Session.IsValid() || !LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSendPlayerReport] OSS session/identity unavailable."));
        return;
    }

#if P2PMODE
    const FName ActiveSessionName = LobbyName;
#else
    // Server-mode: project shadows the inherited AGameSession::SessionName
    // default with the literal "SessionName" (see AEOSGameSession.h). The
    // client's JoinSession at EOSPlayerController.cpp:509 mirrors that name,
    // so the OSS named-session lookup uses the same key on both sides.
    const FName ActiveSessionName = TEXT("SessionName");
#endif

    FNamedOnlineSession* const Named = Session->GetNamedSession(ActiveSessionName);
    if (!Named)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSendPlayerReport] No active named session '%s' - join a lobby/session first."),
            *ActiveSessionName.ToString());
        return;
    }

    // Mode-conditional roster iteration on OSS data (no engine-level
    // PlayerArray needed):
    //   P2P:    Named->SessionSettings.MemberSettings - published by the
    //           OSS-EOS lobby flow.
    //   Server: Named->RegisteredPlayers - populated locally on each client
    //           via AEOSPlayerState::OnRep_UniqueId as PlayerStates replicate.
    FUniqueNetIdPtr TargetNetId;
#if P2PMODE
    for (const TPair<FUniqueNetIdRef, FSessionSettings>& MemberPair : Named->SessionSettings.MemberSettings)
    {
        if (*MemberPair.Key != *LocalNetId)
        {
            TargetNetId = MemberPair.Key;
            break;
        }
    }
#else
    for (const FUniqueNetIdRef& Id : Named->RegisteredPlayers)
    {
        if (*Id != *LocalNetId)
        {
            TargetNetId = Id;
            break;
        }
    }
#endif
    if (!TargetNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSendPlayerReport] No other players in session '%s'."),
            *ActiveSessionName.ToString());
        return;
    }
    const FUniqueNetIdRef Target = TargetNetId.ToSharedRef();

    // If caller passed an empty Context, build a tiny JSON blob so the SDK's
    // "Context must be valid JSON" requirement is visibly demonstrated.
    FString ContextJson = Context;
    if (ContextJson.IsEmpty())
    {
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer
            = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ContextJson);
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("session"), ActiveSessionName.ToString());
        Writer->WriteValue(TEXT("source"),  TEXT("TestSendPlayerReport"));
        Writer->WriteObjectEnd();
        Writer->Close();
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::TestSendPlayerReport] Reporting %s in session '%s' - Message=\"%s\""),
        *Target->ToDebugString(), *ActiveSessionName.ToString(), *Message);

    SendPlayerReport(LocalNetId.ToSharedRef(), Target, Message, ContextJson);
}

// =====================================================================
// Tutorial 9: EOS Sanctions - query active sanctions and submit
// appeals via IOnlineSubsystemEOS::GetPlayerSanctionEOSInterface
// (EOS-only, no generic OSS abstraction). Mode-agnostic.
// RESTRICT_MATCHMAKING is one of the standard EOS sanction actions;
// developers can also define custom actions in EOS Dev Portal.
// =====================================================================

void AEOSPlayerController::GetSanctions(const FUniqueNetId& UserId)
{
    // Tutorial 9: Async query; sets bRestrictMatchmaking on
    // RESTRICT_MATCHMAKING. Chains to FindSessions in the callback.
    IOnlineSubsystemEOS* const EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld()));
    IOnlinePlayerSanctionEOSPtr Sanctions = EOSSubsystem ? EOSSubsystem->GetPlayerSanctionEOSInterface() : nullptr;
    if (!Sanctions.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::GetSanctions] PlayerSanction EOS interface null."));
        // Don't strand the post-login chain - find a session anyway.
        FindSessions();
        return;
    }

    // Local == Target: querying our own sanctions. AsShared() keeps the
    // NetId alive across the async callback.
    Sanctions->QueryActivePlayerSanctions(UserId, UserId,
        FOnQueryActivePlayerSanctionsComplete::CreateLambda(
            [this, PlayerId = UserId.AsShared()](const bool bWasSuccessful)
            {
                IOnlineSubsystemEOS* const EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld()));
                IOnlinePlayerSanctionEOSPtr Sanctions = EOSSubsystem ? EOSSubsystem->GetPlayerSanctionEOSInterface() : nullptr;

                if (bWasSuccessful && Sanctions.IsValid())
                {
                    TArray<IOnlinePlayerSanctionEOS::FOnlinePlayerSanction> Active;
                    if (Sanctions->GetCachedActivePlayerSanctions(PlayerId.Get(), Active) == EOnlineCachedResult::Success)
                    {
                        UE_LOG(LogEOSOSSTutorial, Verbose,
                            TEXT("[AEOSPlayerController::GetSanctions] Found %d active sanction(s) for local player."),
                            Active.Num());

                        for (const IOnlinePlayerSanctionEOS::FOnlinePlayerSanction& Sanction : Active)
                        {
                            // Only action this tutorial handles. Real games
                            // would map additional actions to feature toggles.
                            if (Sanction.Action == TEXT("RESTRICT_MATCHMAKING"))
                            {
                                bRestrictMatchmaking = true;
                                UE_LOG(LogEOSOSSTutorial, Warning,
                                    TEXT("[AEOSPlayerController::GetSanctions] RESTRICT_MATCHMAKING active (ref %s) - online play disabled."),
                                    *Sanction.ReferenceId);
                                break;
                            }
                        }
                    }
                }
                else if (!bWasSuccessful)
                {
                    UE_LOG(LogEOSOSSTutorial, Error,
                        TEXT("[AEOSPlayerController::GetSanctions] QueryActivePlayerSanctions failed."));
                }

                // Continue the chain - FindSessions enforces the gate.
                FindSessions();
            }));
}

void AEOSPlayerController::CreateSanctionAppeal(const FUniqueNetId& UserId, FString ReferenceId)
{
    // Tutorial 9: Not auto-fired - learners hook this into a UI.
    // TestCreateSanctionAppeal below is the manual trigger.
    IOnlineSubsystemEOS* const EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld()));
    IOnlinePlayerSanctionEOSPtr Sanctions = EOSSubsystem ? EOSSubsystem->GetPlayerSanctionEOSInterface() : nullptr;
    if (!Sanctions.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::CreateSanctionAppeal] PlayerSanction EOS interface null."));
        return;
    }

    IOnlinePlayerSanctionEOS::FPlayerSanctionAppealSettings Settings;
    // Hardcoded reason - real games bind a dropdown to
    // EPlayerSanctionAppealReason. SDK enum spelling preserved.
    Settings.Reason = IOnlinePlayerSanctionEOS::EPlayerSanctionAppealReason::AppealForForgivenesss;
    Settings.ReferenceId = ReferenceId;

    Sanctions->CreatePlayerSanctionAppeal(UserId, MoveTemp(Settings),
        FOnCreatePlayerSanctionAppealComplete::CreateLambda(
            [Ref = ReferenceId](const bool bWasSuccessful)
            {
                if (bWasSuccessful)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose,
                        TEXT("[AEOSPlayerController::CreateSanctionAppeal] Appeal submitted (ref %s)."),
                        *Ref);
                }
                else
                {
                    UE_LOG(LogEOSOSSTutorial, Error,
                        TEXT("[AEOSPlayerController::CreateSanctionAppeal] Appeal submission failed (ref %s)."),
                        *Ref);
                }
            }));
}

void AEOSPlayerController::TestCreateSanctionAppeal(const FString& ReferenceId)
{
    // Tutorial 9: Console wrapper. Resolves the local NetId so the
    // Exec call can target it.
    if (ReferenceId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestCreateSanctionAppeal] ReferenceId required - copy it from the active sanction in Dev Portal."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestCreateSanctionAppeal] No local NetId - log in first."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::TestCreateSanctionAppeal] Submitting appeal for ref %s."),
        *ReferenceId);

    CreateSanctionAppeal(*LocalNetId, ReferenceId);
}
// Tutorial 11: Voice chat via EOS RTC, dedicated-server branch (P2PMODE=0).
//
// Flow: server mints a per-player join token by calling the EOS Voice Web API (see
// AEOSGameSession::RequestVoiceCredentialsForPlayer), pushes it to the client over
// Client_ReceiveVoiceCredentials, and the client runs IVoiceChat Connect -> Login -> JoinChannel.
// Voice media flows directly between clients and EOS RTC servers; the game server never
// handles voice data. Teardown on EndPlay: LeaveChannel -> Logout -> ReleaseUser -> Disconnect.
// Requires `Voice:createRoomToken` enabled on the Server client in the EOS Dev Portal.
//
// Preferred alternative: extend the EOSVoiceChat plugin with a server-facing
// QueryJoinRoomToken wrapper around EOS_RTCAdmin_QueryJoinRoomToken. The plugin already owns
// the EOS platform handle and artifact config, so gameplay code wouldn't need to touch the
// SDK or UEOSSettings at all - this whole Voice Web API scaffolding would collapse into a
// single plugin call. Calling EOS_RTCAdmin_QueryJoinRoomToken directly from game code is
// another option but pulls SDK headers into gameplay; the Web API keeps the game module
// plugin-layer-only at the cost of one extra REST round trip.
//
// For offline/prototype use only, IVoiceChatUser::InsecureGetJoinToken +
// [EOSVoiceChat]InsecureClientBaseUrl lets the client mint its own forgeable token. Not
// usable against normal deployments - do not ship.

#if !P2PMODE

void AEOSPlayerController::Client_ReceiveVoiceCredentials_Implementation(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl)
{
    // This runs on the owning client of the controller. The server calls this RPC for each player
    // after a successful createRoomToken Web API response; the args carry the signed token + media
    // URL needed to actually join the EOS RTC room.

    // Opt-in guard - the class member is the single source of truth for this session.
    if (!bVoiceChatEnabled)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::Client_ReceiveVoiceCredentials] Voice skipped (bVoiceChatEnabled=false)."));
        return;
    }

    if (RoomName.IsEmpty() || ParticipantToken.IsEmpty() || ClientBaseUrl.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Client_ReceiveVoiceCredentials] Server delivered incomplete voice credentials (Room='%s', Token len=%d, Url len=%d). Skipping."), *RoomName, ParticipantToken.Len(), ClientBaseUrl.Len());
        return;
    }

    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (!VoiceChat)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Client_ReceiveVoiceCredentials] IVoiceChat::Get() returned null - is the VoiceChat / EOSVoiceChat plugin enabled?"));
        return;
    }

    // Cache for the async Connect -> Login -> JoinChannel chain; consumed in HandleVoiceChatLoginComplete.
    CurrentVoiceRoomName       = RoomName;
    CurrentVoiceToken          = ParticipantToken;
    CurrentVoiceClientBaseUrl  = ClientBaseUrl;

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::Client_ReceiveVoiceCredentials] Received credentials for room '%s' (url host len=%d, token len=%d)."), *CurrentVoiceRoomName, CurrentVoiceClientBaseUrl.Len(), CurrentVoiceToken.Len());

    // Share OSS's EOS platform with the voice plugin. Otherwise FEOSVoiceChat::Initialize tries
    // to CreatePlatform from [EOSVoiceChat] config, which this project doesn't populate
    // (credentials live under [/Script/OnlineSubsystemEOS.EOSSettings] artifacts instead).
    if (!VoiceChat->IsInitialized())
    {
        // Type-guard the static_cast: voice fails silently if EOS isn't the active OSS, so we
        // check the subsystem name here even though nothing else in this tutorial does.
        IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
        IOnlineSubsystemEOS* EOSSubsystem = (Subsystem && Subsystem->GetSubsystemName() == EOS_SUBSYSTEM) ? static_cast<IOnlineSubsystemEOS*>(Subsystem) : nullptr;
        const IEOSPlatformHandlePtr PlatformHandle = EOSSubsystem ? EOSSubsystem->GetEOSPlatformHandle() : nullptr;
        if (PlatformHandle.IsValid())
        {
            static_cast<FEOSVoiceChat*>(VoiceChat)->SetPlatformHandle(PlatformHandle);
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSPlayerController::Client_ReceiveVoiceCredentials] OSS EOS platform handle unavailable - voice will try to create its own platform from [EOSVoiceChat] config and likely fail."));
        }
        VoiceChat->Initialize();
    }

    if (VoiceChat->IsConnected())
    {
        // Already connected from a previous session - skip straight to the login step with a synthesized success result.
        HandleVoiceChatConnectComplete(FVoiceChatResult::CreateSuccess());
    }
    else
    {
        VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateUObject(
            this, &ThisClass::HandleVoiceChatConnectComplete));
    }
}

void AEOSPlayerController::HandleVoiceChatConnectComplete(const FVoiceChatResult& Result)
{
    if (!Result.IsSuccess())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] Connect failed: %s"), *Result.ErrorDesc);
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] Connected."));

    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (!VoiceChat)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] IVoiceChat null after connect."));
        return;
    }

    // Resolve the player name (ProductUserId string) and PlatformUserId for the Login call.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] No local net id - cannot log in to voice."));
        return;
    }

    // IVoiceChatUser::Login's PlayerName must be the bare ProductUserId; FUniqueNetId::ToString
    // returns the composite "<EAS>|<PUID>" form which EOS RTC rejects. Type-guard the cast: the
    // rest of this tutorial assumes EOS is the active OSS without checking; the voice path fails
    // silently if EOS isn't active, so guard here to fail loudly instead.
    if (LocalNetId->GetType() != EOS_SUBSYSTEM)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] LocalNetId is not an EOS id (type=%s)."), *LocalNetId->GetType().ToString());
        return;
    }
    const IUniqueNetIdEOS& EosLocalNetId = static_cast<const IUniqueNetIdEOS&>(*LocalNetId);
    const FString VoicePlayerName = LexToString(EosLocalNetId.GetProductUserId());

    // Allocate the IVoiceChatUser only after the identity guard succeeds, so a failed lookup
    // doesn't leave an unused user pending for the LeaveVoiceChat chain to release.
    if (!VoiceChatUser)
    {
        VoiceChatUser = VoiceChat->CreateUser();
    }
    if (!VoiceChatUser)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatConnectComplete] CreateUser returned null."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    const FPlatformUserId PlatformUser = LocalPlayer ? LocalPlayer->GetPlatformUserId() : PLATFORMUSERID_NONE;

    VoiceChatUser->Login(PlatformUser, VoicePlayerName, TEXT(""),
        FOnVoiceChatLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleVoiceChatLoginComplete));
}

void AEOSPlayerController::HandleVoiceChatLoginComplete(const FString& InPlayerName, const FVoiceChatResult& Result)
{
    if (!Result.IsSuccess())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatLoginComplete] Login failed for %s: %s"), *InPlayerName, *Result.ErrorDesc);
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleVoiceChatLoginComplete] Logged in as %s."), *InPlayerName);

    if (!VoiceChatUser)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatLoginComplete] VoiceChatUser null - cannot join channel."));
        return;
    }

    // JoinChannel's ChannelCredentials arg is the FEOSVoiceChatChannelCredentials JSON blob.
    FEOSVoiceChatChannelCredentials Credentials;
    Credentials.ClientBaseUrl    = CurrentVoiceClientBaseUrl;
    Credentials.ParticipantToken = CurrentVoiceToken;
    const FString CredentialsJson = Credentials.ToJson(/*bPretty*/ false);

    VoiceChatUser->JoinChannel(
        CurrentVoiceRoomName,
        CredentialsJson,
        EVoiceChatChannelType::NonPositional,
        FOnVoiceChatChannelJoinCompleteDelegate::CreateUObject(this, &ThisClass::HandleVoiceChatJoinChannelComplete));
}

void AEOSPlayerController::HandleVoiceChatJoinChannelComplete(const FString& ChannelName, const FVoiceChatResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::HandleVoiceChatJoinChannelComplete] Joined channel %s."), *ChannelName);

        // Tutorial 11: now that we're in the channel, bind the speaking-state notification.
        BindVoiceChatPlayerTalkingNotification();
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::HandleVoiceChatJoinChannelComplete] Join channel %s failed: %s"), *ChannelName, *Result.ErrorDesc);
    }
}

void AEOSPlayerController::LeaveVoiceChat()
{
    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (!VoiceChat)
    {
        return;
    }

    // Chain LeaveChannel -> Logout -> ReleaseUser -> Disconnect via completion callbacks. Firing
    // them in parallel races the plugin's state machine - Disconnect clears login state before
    // the RTC LeaveRoom reply lands, tripping an assertion in EOSVoiceChatUser::OnLeaveRoom.
    // Lambdas capture plugin singletons by value only, never `this` - the controller may be
    // destroyed before the chain finishes.
    IVoiceChatUser* const LocalUser   = VoiceChatUser;
    const FString         RoomToLeave = CurrentVoiceRoomName;
    VoiceChatUser = nullptr;
    CurrentVoiceRoomName.Empty();
    CurrentVoiceToken.Empty();
    CurrentVoiceClientBaseUrl.Empty();

    auto DisconnectStep = [VoiceChat]()
    {
        if (VoiceChat->IsConnected())
        {
            VoiceChat->Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateLambda(
                [](const FVoiceChatResult& Result)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::LeaveVoiceChat] Disconnect: %s"), Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
                }));
        }
    };

    auto LogoutAndReleaseStep = [VoiceChat, LocalUser, DisconnectStep]()
    {
        if (LocalUser)
        {
            LocalUser->Logout(FOnVoiceChatLogoutCompleteDelegate::CreateLambda(
                [VoiceChat, LocalUser, DisconnectStep](const FString& PlayerName, const FVoiceChatResult& Result)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::LeaveVoiceChat] Logout %s: %s"), *PlayerName, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
                    // ReleaseUser is safe to call synchronously here - Logout has fully drained the
                    // login-state machine by the time this callback fires.
                    VoiceChat->ReleaseUser(LocalUser);
                    DisconnectStep();
                }));
        }
        else
        {
            DisconnectStep();
        }
    };

    if (LocalUser && !RoomToLeave.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::LeaveVoiceChat] Leaving channel %s."), *RoomToLeave);
        LocalUser->LeaveChannel(RoomToLeave,
            FOnVoiceChatChannelLeaveCompleteDelegate::CreateLambda(
                [LogoutAndReleaseStep](const FString& LeftChannel, const FVoiceChatResult& Result)
                {
                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::LeaveVoiceChat] Channel %s left: %s"), *LeftChannel, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
                    LogoutAndReleaseStep();
                }));
    }
    else
    {
        LogoutAndReleaseStep();
    }

    // Note: do NOT call VoiceChat->Uninitialize() - IVoiceChat is a process-wide singleton
    // shared with any other voice user in the game. Leave it initialized.
}

// ----------------------------------------------------------------------------------------------
// Tutorial 10: Client-side anti-cheat glue (dedicated-server branch).
// ----------------------------------------------------------------------------------------------

#if ACMODE
void AEOSPlayerController::BeginAntiCheatClientSession()
{
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!Client)
    {
        // Verbose, not Warning: in Editor builds this is expected (the plugin logs the
        // reason once at startup). If it fires in a packaged build that's a real issue,
        // but the per-BeginPlay spam would drown useful logs - bump verbosity if you need it.
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::BeginAntiCheatClientSession] AntiCheat client interface unavailable."));
        return;
    }

    // Resolve the local player's PUID for the SDK's BeginSession call.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!LocalNetId.IsValid())
    {
        // Expected when called from the first BeginPlay (login is async, NetId not ready yet).
        // The post-login call from HandleLoginCompleted hits this path with a valid NetId.
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::BeginAntiCheatClientSession] Skipping: no local net id yet (expected pre-login)."));
        return;
    }

    AntiCheatMessageToServerHandle = Client->OnMessageToServer().AddUObject(this, &ThisClass::HandleAntiCheatMessageToServer);
    Client->BeginSession(IEOSAntiCheatClient::EMode::ClientServer, LocalNetId.ToSharedRef());
}

void AEOSPlayerController::EndAntiCheatClientSession()
{
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (!Client)
    {
        return;
    }

    Client->OnMessageToServer().Remove(AntiCheatMessageToServerHandle);
    AntiCheatMessageToServerHandle.Reset();
    Client->EndSession();
}

void AEOSPlayerController::HandleAntiCheatMessageToServer(const TArray<uint8>& Bytes)
{
    Server_AntiCheatMessage(Bytes);
}
#endif // ACMODE

// Server_NotifyAntiCheatReady_Implementation moved to the cross-mode AC section below.

void AEOSPlayerController::Server_AntiCheatMessage_Implementation(const TArray<uint8>& Bytes)
{
#if ACMODE
    // Runs on the server. Feed the bytes back into the server's AntiCheat SDK.
    if (!PlayerState)
    {
        return;
    }
    const FUniqueNetIdRepl& NetIdRepl = PlayerState->GetUniqueId();
    FUniqueNetIdPtr NetIdPtr = NetIdRepl.GetUniqueNetId();
    if (!NetIdPtr.IsValid())
    {
        return;
    }

    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatServer* Server = AntiCheat ? AntiCheat->GetServer() : nullptr;
    if (Server)
    {
        Server->ReceiveMessageFromClient(NetIdPtr.ToSharedRef(), Bytes);
    }
#endif
}

void AEOSPlayerController::Client_AntiCheatMessage_Implementation(const TArray<uint8>& Bytes)
{
#if ACMODE
    // Runs on the owning client. Feed the bytes into the local AntiCheat SDK.
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
    if (Client)
    {
        Client->ReceiveMessageFromServer(Bytes);
    }
#endif
}

void AEOSPlayerController::Server_PeerViolationDetected_Implementation(const FUniqueNetIdRepl& OffendingPlayer, const FString& Reason)
{
    // Tutorial 10: Server-mode stub. Peer-detected violations only fire in P2PMODE.
}

#endif // !P2PMODE

// =====================================================================
// Tutorial 11: Speaking-state notification (cross-mode).
//
// Subscribes to IVoiceChatUser::OnVoiceChatPlayerTalkingUpdated and logs
// each (channel, player, isTalking) edge. Works in both topologies:
//
// - P2PMODE=0 (dedicated server): the local user is the one we created
//   explicitly via IVoiceChat::CreateUser in HandleVoiceChatConnectComplete
//   (see VoiceChatUser member).
//
// - P2PMODE=1 (listen-server-over-P2P with lobby voice): the OSS-EOS
//   plugin auto-manages the voice user as part of the lobby flow
//   (bUseLobbiesVoiceChatIfAvailable=true on the lobby session settings).
//   We retrieve it via IOnlineSubsystem::GetVoiceChatUserInterface, which
//   lazy-creates + Logs in the user on first call and caches it keyed by
//   LocalUserId.
//
// Real games would drive a HUD speaking indicator from this; we just log.
// =====================================================================

IVoiceChatUser* AEOSPlayerController::GetActiveVoiceChatUser() const
{
#if P2PMODE
    // Cast IOnlineSubsystem -> IOnlineSubsystemEOS - GetVoiceChatUserInterface is an
    // EOS-specific extension on the OSS-EOS plugin, not on the generic OSS interface.
    IOnlineSubsystemEOS* const EOSSubsystem = static_cast<IOnlineSubsystemEOS*>(Online::GetSubsystem(GetWorld()));
    if (!EOSSubsystem)
    {
        return nullptr;
    }
    IOnlineIdentityPtr Identity = EOSSubsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        return nullptr;
    }
    FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
    if (!LocalNetId.IsValid() || !LocalNetId->IsValid())
    {
        return nullptr;
    }
    return EOSSubsystem->GetVoiceChatUserInterface(*LocalNetId);
#else
    return VoiceChatUser;
#endif
}

void AEOSPlayerController::BindVoiceChatPlayerTalkingNotification()
{
    if (bVoiceTalkingNotificationBound)
    {
        return;
    }

    IVoiceChatUser* User = GetActiveVoiceChatUser();
    if (!User)
    {
        // Voice user not ready yet on this caller's hook - another caller
        // (BeginPlay / JoinChannel-complete / lobby-create-complete) will
        // retry once voice is up.
        return;
    }

    User->OnVoiceChatPlayerTalkingUpdated().AddUObject(this, &ThisClass::HandleVoiceChatPlayerTalkingUpdated);
    bVoiceTalkingNotificationBound = true;

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerController::BindVoiceChatPlayerTalkingNotification] Bound speaking-state notification."));
}

void AEOSPlayerController::HandleVoiceChatPlayerTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking)
{
    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[Voice] %s %s talking in channel %s."),
        *PlayerName,
        bIsTalking ? TEXT("started") : TEXT("stopped"),
        *ChannelName);
}

// =====================================================================
// Tutorial 10: AC handshake (cross-mode).
//
// Single CopyLocalIdToken call (joiner side) and single VerifyIdToken
// call (authority side) for both P2PMODE=0 (dedicated server) and
// P2PMODE=1 (listen-server host). The post-verify register call
// branches: !P2PMODE -> AC Server's RegisterClient via AEOSGameSession;
// P2PMODE -> host's local AC Client RegisterPeer.
// =====================================================================

void AEOSPlayerController::SendIdTokenForVerification()
{
#if ACMODE
    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;

    FString IdTokenJwt;
    if (AntiCheat && LocalNetId.IsValid())
    {
        AntiCheat->CopyLocalIdToken(LocalNetId.ToSharedRef(), IdTokenJwt);
    }
    Server_NotifyAntiCheatReady(IdTokenJwt);
#endif
}

void AEOSPlayerController::Server_NotifyAntiCheatReady_Implementation(const FString& IdTokenJwt)
{
#if ACMODE
    // Verify the joiner's JWT before trusting the PUID for RegisterClient (!P2PMODE) /
    // RegisterPeer (P2PMODE). Single VerifyIdToken callsite for both modes.
    if (!PlayerState) { return; }
    FUniqueNetIdPtr ClaimedNetIdPtr = PlayerState->GetUniqueId().GetUniqueNetId();
    if (!ClaimedNetIdPtr.IsValid()) { return; }
    const FUniqueNetIdRef ClaimedNetId = ClaimedNetIdPtr.ToSharedRef();

    IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
    if (!AntiCheat)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Server_NotifyAntiCheatReady] AntiCheat plugin unavailable."));
        return;
    }

    TWeakObjectPtr<AEOSPlayerController> WeakThis(this);
    AntiCheat->VerifyIdToken(ClaimedNetId, IdTokenJwt, IEOSAntiCheat::FOnIdTokenVerified::CreateLambda(
        [WeakThis, ClaimedNetId](bool bVerifySucceeded)
        {
            AEOSPlayerController* PC = WeakThis.Get();
            if (!PC) { return; }

            AGameModeBase* GM = PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode() : nullptr;
            AGameSession* GS = GM ? GM->GameSession : nullptr;
            if (!GS) { return; }

            if (!bVerifySucceeded)
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::Server_NotifyAntiCheatReady] IdToken verify failed for %s - kicking."), *ClaimedNetId->ToString());
                GS->KickPlayer(PC, FText::FromString(TEXT("AntiCheat: IdToken verify failed")));
                return;
            }

            // Verified - register the joiner with AntiCheat. Mode-specific:
#if !P2PMODE
            // Dedicated-server: hand off to AEOSGameSession, which calls Server->RegisterClient.
            if (AEOSGameSession* EosGameSession = Cast<AEOSGameSession>(GS))
            {
                EosGameSession->RegisterAntiCheatClient(ClaimedNetId);
            }
#else
            // Listen-server host: RegisterPeer the joiner on host's local AC Client. Other
            // peers RegisterPeer this joiner via the lobby events with no JWT verify (acceptable
            // because they're not the authority - see EAC PeerToPeer trust model). Re-fetch
            // IEOSAntiCheat::Get() inside the lambda - the singleton is stable, but the outer
            // AntiCheat local isn't in the capture list (lambda captures only WeakThis + ClaimedNetId).
            if (IEOSAntiCheat* AC = IEOSAntiCheat::Get())
            {
                if (IEOSAntiCheatClient* Client = AC->GetClient())
                {
                    Client->RegisterPeer(ClaimedNetId);
                }
            }
#endif
        }));
#endif // ACMODE
}


void AEOSPlayerController::SendSessionInvite(const FUniqueNetIdRef& Target)
{
    // Tutorial 5 + 7: Send a session/lobby invite to a specific PUID.
    // Mode-agnostic - the OSS routes to EOS_Sessions_SendInvite for
    // server-mode and EOS_Lobby_SendInvite for P2P based on the named
    // session's flavor (bUseLobbiesIfAvailable).
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!Session.IsValid() || !LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::SendSessionInvite] OSS session/identity unavailable."));
        return;
    }

#if P2PMODE
    const FName ActiveSessionName = LobbyName;
#else
    const FName ActiveSessionName = TEXT("SessionName");
#endif

    if (!Session->GetNamedSession(ActiveSessionName))
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::SendSessionInvite] No active named session '%s' - join a lobby/session first."),
            *ActiveSessionName.ToString());
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::SendSessionInvite] Inviting %s to '%s'."),
        *Target->ToDebugString(), *ActiveSessionName.ToString());

    if (!Session->SendSessionInviteToFriend(*LocalNetId, ActiveSessionName, *Target))
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::SendSessionInvite] SendSessionInviteToFriend call failed - target may not have played this game (no EOS Connect record)."));
    }
}

void AEOSPlayerController::TestSendSessionInvite(const FString& TargetProductUserId)
{
    // Tutorial 5 + 7: Console wrapper - resolve a raw PUID string into
    // an FUniqueNetIdRef via the identity interface, then forward to
    // SendSessionInvite. Avoids the friends-list dance (a separate
    // audit candidate).
    if (TargetProductUserId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSendSessionInvite] ProductUserId required - copy it from the other client's Login succeeded log line."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    if (!Identity.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSendSessionInvite] Identity interface null."));
        return;
    }

    // OSS-EOS expects the FUniqueNetIdEOS string form "<EpicAccount>|<PUID>".
    // For invite targets we only need the PUID half - prefix with the
    // separator so CreateUniquePlayerId parses it as PUID-only.
    const FString NetIdString = FString::Printf(TEXT("|%s"), *TargetProductUserId);
    FUniqueNetIdPtr Target = Identity->CreateUniquePlayerId(NetIdString);
    if (!Target.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSendSessionInvite] CreateUniquePlayerId failed for '%s'."),
            *TargetProductUserId);
        return;
    }

    SendSessionInvite(Target.ToSharedRef());
}

void AEOSPlayerController::HandleSessionUserInviteAccepted(
    const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId,
    const FOnlineSessionSearchResult& InviteResult)
{
    // Tutorial 5 + 7: Fired when the local user accepts an invite via
    // the EOS Social Overlay (or any other accept path). The InviteResult
    // is already a resolved FOnlineSessionSearchResult, so we skip
    // FindSessions and go straight to JoinSession.
    if (!bWasSuccessful || !InviteResult.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleSessionUserInviteAccepted] Invite accepted but result invalid (bWasSuccessful=%d)."),
            bWasSuccessful ? 1 : 0);
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::HandleSessionUserInviteAccepted] Invite accepted - joining."));

    SessionToJoin = InviteResult;

    // Resolve the connect string the same way HandleFindSessionsCompleted does.
    // Server-mode HandleJoinSessionCompleted overrides this to 127.0.0.1:7777,
    // but the P2P branch uses ConnectString directly for ClientTravel - so it
    // MUST be populated for the invite-accept path too, not just FindSessions.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (Session.IsValid())
    {
        Session->GetResolvedConnectString(InviteResult, NAME_GamePort, ConnectString);
    }

    JoinSession();
}

void AEOSPlayerController::HandleSessionInviteReceived(
    const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId,
    const FOnlineSessionSearchResult& InviteResult)
{
    // Tutorial 5 + 7: Fired when the SDK receives an invite for the local
    // user. The EOS Social Overlay handles the accept popup; for testing
    // without the overlay we cache the InviteResult so TestAcceptLastInvite
    // can replay it. Real games would surface a custom in-game UI bound
    // to the same cached result.
    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::HandleSessionInviteReceived] Invite from %s (valid=%d)."),
        *FromId.ToDebugString(), InviteResult.IsValid() ? 1 : 0);

    if (InviteResult.IsValid())
    {
        LastReceivedInvite = InviteResult;
    }
}

void AEOSPlayerController::TestAcceptLastInvite()
{
    // Tutorial 5 + 7: Console accept path for the most recent invite.
    // Bypasses the EOS Social Overlay - useful for headless tests or
    // when the overlay isn't visible (recording, fullscreen, etc.).
    if (!LastReceivedInvite.IsSet() || !LastReceivedInvite->IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestAcceptLastInvite] No cached invite - send one with TestSendSessionInvite from the other client first."));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::TestAcceptLastInvite] Accepting cached invite via JoinSession."));

    SessionToJoin = LastReceivedInvite.GetValue();

    // Resolve the connect string. Required for the P2P ClientTravel path
    // and harmless in server-mode (HandleJoinSessionCompleted overrides it).
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (Session.IsValid())
    {
        Session->GetResolvedConnectString(LastReceivedInvite.GetValue(), NAME_GamePort, ConnectString);
    }

    // Consume the cache - re-accepting the same invite would re-trigger
    // the join, which the OSS would reject as already-joined.
    LastReceivedInvite.Reset();

    JoinSession();
}


void AEOSPlayerController::TestLogout()
{
    // Tutorial 1: Console wrapper for Logout. Client-only - server has
    // no local user.
    if (IsRunningDedicatedServer())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestLogout] Client-only API; ignored on server."));
        return;
    }
    Logout();
}

// =====================================================================
// Tutorial 4: P2P lobby attribute + member-attribute updates via
// IOnlineSession::UpdateSession. Two paths:
//   - Lobby Settings: the lobby host owns. Joiners calling
//     TestSetLobbyAttribute will get an OSS error.
//   - MemberSettings: any member can update their own per-member
//     attribute via TestSetMyMemberAttribute.
// Server-mode session attributes are NOT exposed via Exec - the
// dedicated server has no console, and a client RPC for this isn't
// representative of real games. Instead, see AEOSGameSession's
// Phase=Lobby/InProgress flow: server-driven update on first player
// join (Tutorial 5).
// =====================================================================

void AEOSPlayerController::HandleUpdateSessionCompleted(FName SessionName, bool bWasSuccessful)
{
    // Tutorial 5 + 4 + 7: Shared completion handler for all UpdateSession
    // calls. Bound per-call by the path that initiated the update.
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[AEOSPlayerController::HandleUpdateSessionCompleted] UpdateSession succeeded for '%s'."),
            *SessionName.ToString());
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::HandleUpdateSessionCompleted] UpdateSession failed for '%s'."),
            *SessionName.ToString());
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (Session.IsValid() && UpdateSessionDelegateHandle.IsValid())
    {
        Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
        UpdateSessionDelegateHandle.Reset();
    }
}

void AEOSPlayerController::TestSetLobbyAttribute(const FString& Key, const FString& Value)
{
    // Tutorial 4: P2P lobby attribute update. Host runs UpdateSession on
    // the lobby's overall Settings map; joiners calling this get an
    // OSS error since they don't own the lobby.
#if !P2PMODE
    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetLobbyAttribute] P2P only. In server mode, use TestSetSessionAttribute."));
    return;
#else
    if (IsRunningDedicatedServer())
    {
        return;
    }
    if (Key.IsEmpty() || Value.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetLobbyAttribute] Both Key and Value required."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (!Session.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetLobbyAttribute] Session interface null."));
        return;
    }

    FNamedOnlineSession* Named = Session->GetNamedSession(LobbyName);
    if (!Named)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSetLobbyAttribute] No active lobby '%s'."),
            *LobbyName.ToString());
        return;
    }

    FOnlineSessionSettings UpdatedSettings = Named->SessionSettings;
    UpdatedSettings.Set(FName(*Key), Value, EOnlineDataAdvertisementType::ViaOnlineService);

    UpdateSessionDelegateHandle =
        Session->AddOnUpdateSessionCompleteDelegate_Handle(
            FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleUpdateSessionCompleted));

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::TestSetLobbyAttribute] Updating '%s': %s = %s"),
        *LobbyName.ToString(), *Key, *Value);

    if (!Session->UpdateSession(LobbyName, UpdatedSettings, /*bShouldRefreshOnlineData=*/true))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetLobbyAttribute] UpdateSession call failed (are you the host?)."));
        Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
        UpdateSessionDelegateHandle.Reset();
    }
#endif
}

void AEOSPlayerController::TestSetMyMemberAttribute(const FString& Key, const FString& Value)
{
    // Tutorial 4: P2P lobby per-member attribute update. Each member
    // can update their OWN MemberSettings entry. Used for team /
    // ready-state / class selection that other members can read.
#if !P2PMODE
    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] P2P only. Server-mode sessions don't have MemberSettings."));
    return;
#else
    if (IsRunningDedicatedServer())
    {
        return;
    }
    if (Key.IsEmpty() || Value.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] Both Key and Value required."));
        return;
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
    FUniqueNetIdPtr LocalNetId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    if (!Session.IsValid() || !LocalNetId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] Session/Identity unavailable."));
        return;
    }

    FNamedOnlineSession* Named = Session->GetNamedSession(LobbyName);
    if (!Named)
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] No active lobby '%s'."),
            *LobbyName.ToString());
        return;
    }

    FOnlineSessionSettings UpdatedSettings = Named->SessionSettings;
    FSessionSettings& LocalMemberSettings = UpdatedSettings.MemberSettings.FindOrAdd(LocalNetId.ToSharedRef());
    LocalMemberSettings.Add(FName(*Key), FOnlineSessionSetting(Value, EOnlineDataAdvertisementType::ViaOnlineService));

    UpdateSessionDelegateHandle =
        Session->AddOnUpdateSessionCompleteDelegate_Handle(
            FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleUpdateSessionCompleted));

    UE_LOG(LogEOSOSSTutorial, Verbose,
        TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] Updating my MemberSettings in '%s': %s = %s"),
        *LobbyName.ToString(), *Key, *Value);

    if (!Session->UpdateSession(LobbyName, UpdatedSettings, /*bShouldRefreshOnlineData=*/true))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerController::TestSetMyMemberAttribute] UpdateSession call failed."));
        Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
        UpdateSessionDelegateHandle.Reset();
    }
#endif
}
