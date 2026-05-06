// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSGameSession.h"
#include "EOSPlayerController.h"
#include "GameFramework/PlayerState.h" // UE 5.8: APlayerState is no longer pulled in transitively; include explicitly to dereference ExitingPlayer->PlayerState.
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineStatsInterface.h"

#if !P2PMODE
// Tutorial 11 (server-side voice flow):
#include "EOSSettings.h"                       // UEOSSettings::GetSelectedArtifactSettings - needed to read Server credentials ourselves because EOSVoiceChat exposes no server-side token-issuance API.
#include "OnlineSubsystemEOSTypesPublic.h"     // IUniqueNetIdEOS - exposes GetProductUserId() on OSS-produced FUniqueNetIds.
#include "EOSShared.h"                         // LexToString(EOS_ProductUserId) - formats the PUID as the string the Voice Web API wants.
#include "HttpModule.h"                        // FHttpModule - used for the two Voice-Web-API calls below.
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"                       // FBase64::Encode - HTTP Basic auth header for the OAuth request.
#include "Dom/JsonObject.h"                    // FJsonObject - parse EOS's responses.
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if ACMODE
// Tutorial 10 (server-side AC): wires the lifecycle and the OnViolation -> kick path.
#include "IEOSAntiCheat.h"
#include "IEOSAntiCheatServer.h"
#endif
#endif // !P2PMODE (closes the server-only includes block)

// =====================================================================
// AEOSGameSession is dedicated-server-only - the GameMode only assigns it
// to GameSessionClass when P2PMODE=0 (see EOS_OSS_TutorialGameMode.cpp).
// Each function below internally wraps its body in #if !P2PMODE so P2P
// builds compile only the empty function shells UHT needs for vtable
// population (constructor + AGameSession virtual overrides). Reading a
// function in isolation, the inner #if !P2PMODE accurately represents
// "the whole body is dedicated-server-only", not a per-line mode split.
// =====================================================================


AEOSGameSession::AEOSGameSession()
{
    // Tutorial 5: Including constructor here for clarity. Nothing added in derived class for this tutorial.
}

bool AEOSGameSession::ProcessAutoLogin()
{
#if !P2PMODE
    // Tutorial 5: Override base function as players need to login before joining the session. We don't want to call AutoLogin on server.
    return true;
#else
    return Super::ProcessAutoLogin();
#endif
}

FString AEOSGameSession::ApproveLogin(const FString& Options)
{
#if !P2PMODE
    if (IsRunningDedicatedServer())
    {
        // Let the base class veto the login first (banned, malformed options, etc.) and propagate that rejection.
        const FString SuperResult = Super::ApproveLogin(Options);
        if (!SuperResult.IsEmpty())
        {
            return SuperResult;
        }

        // If the server is full return an error. Catching the error on the client is NOT implemented in this sample.
        // See UCommonSessionSubsystem::Initialize in the Lyra project for an example.
        // Note: NumberOfPlayersInSession is incremented in PostLogin (after approval), so under a concurrent-join burst two approvals could race past `== Max`. Using `>=` keeps the FULL gate closed in that edge case.
        return NumberOfPlayersInSession >= MaxNumberOfPlayersInSession ? "FULL" : "";
    }
    else
    {
        return "";
    }
#else
    return Super::ApproveLogin(Options);
#endif
}


void AEOSGameSession::BeginPlay()
{
#if !P2PMODE
    // Tutorial 5: Override base function to create session when running as dedicated server.
    Super::BeginPlay();

    if (IsRunningDedicatedServer() && !bSessionExists) // Only create a session if running as a dedicated server and session doesn't exist.
    {
        CreateSession("KeyName", "KeyValue"); // Should parametrize Key/Value pair for custom attribute
    }

#if ACMODE
    // Tutorial 10: Spin up the AntiCheat server session so it's ready before any
    // players attempt to register. Wire the violation and message-to-client
    // delegates here; we tear them down in EndPlay.
    if (IsRunningDedicatedServer())
    {
        if (IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get())
        {
            AntiCheatViolationDelegateHandle = AntiCheat->OnViolation().AddUObject(this, &ThisClass::HandleAntiCheatViolation);

            if (IEOSAntiCheatServer* Server = AntiCheat->GetServer())
            {
                AntiCheatMessageToClientDelegateHandle = Server->OnMessageToClient().AddUObject(this, &ThisClass::HandleAntiCheatMessageToClient);
                Server->BeginSession();
            }
        }
    }
#endif
#endif // !P2PMODE
}

void AEOSGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !P2PMODE
    // Tutorial 5: Override base function to destroy session at end of play. Only the dedicated server
    // owns a session (clients join but don't create one here), so skip DestroySession on clients to
    // avoid the spurious failure log that would otherwise fire on every clean client exit.
#if ACMODE
    // Tutorial 10: Tear down AntiCheat before OSS shutdown. Order matches voice: plugin-level teardown first, then OSS. Delegate unbinds are unconditional so stale bindings never outlive this actor.
    if (IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get())
    {
        AntiCheat->OnViolation().Remove(AntiCheatViolationDelegateHandle);
        AntiCheatViolationDelegateHandle.Reset();

        if (IEOSAntiCheatServer* Server = AntiCheat->GetServer())
        {
            Server->OnMessageToClient().Remove(AntiCheatMessageToClientDelegateHandle);
            AntiCheatMessageToClientDelegateHandle.Reset();

            if (IsRunningDedicatedServer())
            {
                Server->EndSession();
            }
        }
    }
#endif
    Super::EndPlay(EndPlayReason);
    if (bSessionExists)
    {
        DestroySession();
    }
#endif // !P2PMODE
}

void AEOSGameSession::PostLogin(APlayerController* NewPlayer)
{
#if !P2PMODE
    Super::PostLogin(NewPlayer);
    NumberOfPlayersInSession++; // Keep track of players registered in session

    // Tutorial 5: Add code here if you need to do anything else after a player joins the dedicated server.
#endif
}

void AEOSGameSession::NotifyLogout(const APlayerController* ExitingPlayer)
{
#if !P2PMODE
    // Tutorial 5: Override base function to handle players leaving EOS Session.
#if ACMODE
    // Tutorial 10: Unregister from AntiCheat before OSS unregister so the SDK
    // doesn't continue heartbeating a player that's already gone. Safe to call
    // unconditionally - the plugin no-ops on unknown PUIDs.
    if (IsRunningDedicatedServer() && ExitingPlayer && ExitingPlayer->PlayerState)
    {
        const FUniqueNetIdRepl& ExitingNetId = ExitingPlayer->PlayerState->GetUniqueId();
        FUniqueNetIdPtr ExitingNetIdPtr = ExitingNetId.GetUniqueNetId();
        if (ExitingNetIdPtr.IsValid())
        {
            if (IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get())
            {
                if (IEOSAntiCheatServer* Server = AntiCheat->GetServer())
                {
                    Server->UnregisterClient(ExitingNetIdPtr.ToSharedRef());
                }
            }
        }
    }
#endif
    Super::NotifyLogout(ExitingPlayer); // This calls UnregisterPlayer

    // When players leave the dedicated server we need to check how many players are left. If 0 players are left, the session is *ended* here (not destroyed - destruction happens in EndPlay).
    if (IsRunningDedicatedServer())
    {
        NumberOfPlayersInSession--; // Keep track of players as they leave

        // No one left in server - end session if session is InProgress.
        if (NumberOfPlayersInSession == 0)
        {
            IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
            IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

            if (Session.IsValid())
            {
                if (Session->GetSessionState(SessionName) == EOnlineSessionState::InProgress)
                {
                    EndSession();
                }
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::NotifyLogout] Session interface null"));
            }
        }
    }
    // No client-side else branch: ClientTravel tears down the outgoing PC and triggers NotifyLogout on the client even though the player is *arriving* at a new server, not leaving. Logging anything here would be misleading.
#endif // !P2PMODE
}

// =====================================================================
// Tutorial 5: Sessions - server-side lifecycle (dedicated server, P2PMODE=0).
//
// Full IOnlineSession server arc: CreateSession -> RegisterPlayer (per
// joiner) -> StartSession (when full) -> EndSession (when empty) ->
// DestroySession (on shutdown). Each step is async; the Handle*Completed
// callbacks chain to the next. Pairs with Tutorial 5's client-side
// FindSessions/JoinSession in EOSPlayerController.cpp.
// =====================================================================

void AEOSGameSession::CreateSession(FName KeyName, FString KeyValue) // Dedicated Server Only
{
#if !P2PMODE
    // Tutorial 5: This function will create an EOS Session.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        // Bind delegate to callback function.
        CreateSessionDelegateHandle =
            Session->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleCreateSessionCompleted));


        // Set session settings.
        TSharedRef<FOnlineSessionSettings> SessionSettings = MakeShared<FOnlineSessionSettings>();
        SessionSettings->NumPublicConnections = MaxNumberOfPlayersInSession; // We will test our sessions with 3 players to keep things simple.
        SessionSettings->bShouldAdvertise = true; // Creates a public match that is searchable by clients.
        SessionSettings->bUsesPresence = false; // No presence on a dedicated server
        SessionSettings->bAllowJoinViaPresence = false;
        SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
        // bAllowInvites=true is required so clients in this session can send
        // invites for it. The flag is server-authoritative: it propagates to
        // clients via the search result and is checked client-side at
        // EOS_Sessions_SendInvite time. The SDK logs a (false-positive)
        // Warning at CreateSession ("Invites Allowed was enabled on a session
        // which is not the presence session") because a dedicated server can
        // never be a presence session by design - no local user, so the SDK
        // forces bUsesPresence=false.
        SessionSettings->bAllowInvites = true;
        SessionSettings->bAllowJoinInProgress = false; // Once the session is started, no one can join.
        SessionSettings->bIsDedicated = true; // Session created on dedicated server.
        SessionSettings->bUseLobbiesIfAvailable = false; // This is an EOS Session not an EOS Lobby as they aren't supported on Dedicated Servers.
        SessionSettings->bUseLobbiesVoiceChatIfAvailable = false; // This is an EOS Session not an EOS Lobby as they aren't supported on Dedicated Servers.
        SessionSettings->bUsesStats = true; // Needed to keep track of player stats.

        // This custom attribute will be used in searches on GameClients.
        SessionSettings->Settings.Add(KeyName, FOnlineSessionSetting((KeyValue), EOnlineDataAdvertisementType::ViaOnlineService));

        // Tutorial 5: Initial Phase attribute. Flipped to "InProgress" on
        // the first player join in HandleRegisterPlayerCompleted - shows
        // how the server changes session state in response to gameplay
        // events (no client RPC; the server is the authority).
        SessionSettings->Settings.Add(FName(TEXT("Phase")),
            FOnlineSessionSetting(FString(TEXT("Lobby")), EOnlineDataAdvertisementType::ViaOnlineService));

        // Tutorial 11: EOS RTC room name for this match. Delivered to clients with their voice credentials.
        VoiceRoomName = SessionName.ToString() + TEXT("_Voice");

        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::CreateSession] Creating session..."));

        if (!Session->CreateSession(0, SessionName, *SessionSettings))
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::CreateSession] CreateSession call failed."));
            Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
            CreateSessionDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::CreateSession] Session interface null"));
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleCreateSessionCompleted(FName EOSSessionName, bool bWasSuccessful) // Dedicated Server Only
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in CreateSession once the session is created (or there is a failure to create).
    if (bWasSuccessful)
    {
        bSessionExists = true;
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleCreateSessionCompleted] Session %s created."), *EOSSessionName.ToString());
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleCreateSessionCompleted] Failed to create session."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
        CreateSessionDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

void AEOSGameSession::RegisterPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, bool bWasFromInvite)
{
#if !P2PMODE
    // Tutorial 5: Authoritative server-side EOS Session register. Clients mirror the roster locally via AEOSPlayerState::OnRep_UniqueId.

    // Super sets PlayerId + UniqueId on the PlayerState (triggers OnRep_UniqueId on clients). Don't drop it.
    Super::RegisterPlayer(NewPlayer, UniqueId, bWasFromInvite);

    if (IsRunningDedicatedServer())
    {
        IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
        IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

        if (Session.IsValid())
        {
            // Bind delegate to callback function.
            RegisterPlayerDelegateHandle =
                Session->AddOnRegisterPlayersCompleteDelegate_Handle(FOnRegisterPlayersCompleteDelegate::CreateUObject(
                    this,
                    &ThisClass::HandleRegisterPlayerCompleted));

            if (!Session->RegisterPlayer(SessionName, *UniqueId, false))
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RegisterPlayer] RegisterPlayer call failed."));
                Session->ClearOnRegisterPlayersCompleteDelegate_Handle(RegisterPlayerDelegateHandle);
                RegisterPlayerDelegateHandle.Reset();
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RegisterPlayer] Session interface null"));
        }
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleRegisterPlayerCompleted(FName EOSSessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful)
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in RegisterPlayer once the player is registered (or there is a failure).
    if (bWasSuccesful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] Player registered in EOS session."));

        // Tutorial 11: Mint voice credentials for each newly-registered player.
        for (const FUniqueNetIdRef& PlayerId : PlayerIds)
        {
            if (AEOSPlayerController* OwningPC = FindPlayerControllerByNetId(PlayerId))
            {
                RequestVoiceCredentialsForPlayer(OwningPC, PlayerId);
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] No PlayerController matched PlayerId=%s - voice credentials will not be issued for this player."), *OSS_REDACT(PlayerId->ToString()));
            }
        }

        // Tutorial 5: Flip the Phase attribute on the 0->1 transition.
        // Demonstrates server-side session-attribute update in response
        // to a real gameplay event (first player join). Other natural
        // triggers in a real game: round start, score milestones, time
        // expiry. Never client-driven - the server has authority.
        if (NumberOfPlayersInSession == 1)
        {
            IOnlineSubsystem* UpdateSubsystem = Online::GetSubsystem(GetWorld());
            IOnlineSessionPtr UpdateSessionInterface = UpdateSubsystem ? UpdateSubsystem->GetSessionInterface() : nullptr;
            if (UpdateSessionInterface.IsValid())
            {
                if (FNamedOnlineSession* Named = UpdateSessionInterface->GetNamedSession(SessionName))
                {
                    FOnlineSessionSettings UpdatedSettings = Named->SessionSettings;
                    UpdatedSettings.Set(FName(TEXT("Phase")), FString(TEXT("InProgress")), EOnlineDataAdvertisementType::ViaOnlineService);

                    UpdateSessionDelegateHandle =
                        UpdateSessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
                            FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleUpdateSessionCompleted));

                    UE_LOG(LogEOSOSSTutorial, Verbose,
                        TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] First player joined - flipping Phase to InProgress."));

                    if (!UpdateSessionInterface->UpdateSession(SessionName, UpdatedSettings))
                    {
                        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] UpdateSession call failed."));
                        UpdateSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
                        UpdateSessionDelegateHandle.Reset();
                    }
                }
            }
        }

        if (NumberOfPlayersInSession == MaxNumberOfPlayersInSession)
        {
            StartSession(); // Start the session when we've reached the max number of players.
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] Failed to register player (async)."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnRegisterPlayersCompleteDelegate_Handle(RegisterPlayerDelegateHandle);
        RegisterPlayerDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleUpdateSessionCompleted(FName EOSSessionName, bool bWasSuccessful)
{
#if !P2PMODE
    // Tutorial 5: UpdateSession completion. Bound per-call from server-side attribute updates (currently the Phase=InProgress flip on first player join).
    // Real games would chain in further state-driven updates from the same handler.
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose,
            TEXT("[AEOSGameSession::HandleUpdateSessionCompleted] UpdateSession succeeded for '%s'."),
            *EOSSessionName.ToString());
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error,
            TEXT("[AEOSGameSession::HandleUpdateSessionCompleted] UpdateSession failed for '%s'."),
            *EOSSessionName.ToString());
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (Session.IsValid() && UpdateSessionDelegateHandle.IsValid())
    {
        Session->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
        UpdateSessionDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

void AEOSGameSession::UnregisterPlayer(const APlayerController* ExitingPlayer)
{
#if !P2PMODE
    // Tutorial 5: Authoritative server-side EOS Session unregister. Clients clear their local mirror in AEOSPlayerState::EndPlay (paired with OnRep_UniqueId).
    Super::UnregisterPlayer(ExitingPlayer);

    if (IsRunningDedicatedServer())
    {
        IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
        IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

        if (Session.IsValid())
        {
            if (ExitingPlayer->PlayerState) // If the player leaves ungracefully this could be null.
            {
                // Bind delegate to callback function.
                UnregisterPlayerDelegateHandle =
                    Session->AddOnUnregisterPlayersCompleteDelegate_Handle(FOnUnregisterPlayersCompleteDelegate::CreateUObject(
                        this,
                        &ThisClass::HandleUnregisterPlayerCompleted));

                // UE 5.0+: APlayerState::UniqueId is private - use the GetUniqueId() accessor, which returns const FUniqueNetIdRepl& (dereferenceable to FUniqueNetId).
                if (!Session->UnregisterPlayer(SessionName, *ExitingPlayer->PlayerState->GetUniqueId()))
                {
                    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::UnregisterPlayer] UnregisterPlayer call failed."));
                    Session->ClearOnUnregisterPlayersCompleteDelegate_Handle(UnregisterPlayerDelegateHandle);
                    UnregisterPlayerDelegateHandle.Reset();
                }
            }
            else
            {
                UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSGameSession::UnregisterPlayer] ExitingPlayer->PlayerState is null - cannot unregister (player may have left ungracefully)."));
            }
        }
        else
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::UnregisterPlayer] Session interface null"));
        }
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleUnregisterPlayerCompleted(FName EOSSessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful)
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in UnregisterPlayer once the player is unregistered (or there is a failure).
    if (bWasSuccesful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleUnregisterPlayerCompleted] Player unregistered from EOS session."));
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleUnregisterPlayerCompleted] Failed to unregister player (async)."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnUnregisterPlayersCompleteDelegate_Handle(UnregisterPlayerDelegateHandle);
        UnregisterPlayerDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

void AEOSGameSession::StartSession()
{
#if !P2PMODE
    // Tutorial 5: This function is called once all players are registered. It will mark the EOS Session as started.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        // Bind delegate to callback function.
        StartSessionDelegateHandle =
            Session->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleStartSessionCompleted));

        if (!Session->StartSession(SessionName))
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::StartSession] StartSession call failed."));
            Session->ClearOnStartSessionCompleteDelegate_Handle(StartSessionDelegateHandle);
            StartSessionDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::StartSession] Session interface null"));
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleStartSessionCompleted(FName EOSSessionName, bool bWasSuccessful)
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in StartSession once the session is started (or there is a failure).
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleStartSessionCompleted] Session started."));
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleStartSessionCompleted] Failed to start session (async)."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnStartSessionCompleteDelegate_Handle(StartSessionDelegateHandle);
        StartSessionDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

void AEOSGameSession::EndSession()
{
#if !P2PMODE
    // Tutorial 5: This function is called once all players have left the session. It will mark the EOS Session as ended.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        // Bind delegate to callback function.
        EndSessionDelegateHandle =
            Session->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleEndSessionCompleted));

        if (!Session->EndSession(SessionName))
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::EndSession] EndSession call failed."));
            Session->ClearOnEndSessionCompleteDelegate_Handle(EndSessionDelegateHandle);
            EndSessionDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::EndSession] Session interface null"));
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleEndSessionCompleted(FName EOSSessionName, bool bWasSuccessful)
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in EndSession once the session is ended (or there is a failure).
    if (bWasSuccessful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleEndSessionCompleted] Session ended."));
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleEndSessionCompleted] Failed to end session (async)."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnEndSessionCompleteDelegate_Handle(EndSessionDelegateHandle);
        EndSessionDelegateHandle.Reset();
    }
#endif // !P2PMODE
}


void AEOSGameSession::DestroySession()
{
#if !P2PMODE
    // Tutorial 5: Called when EndPlay() is called. This will destroy the EOS Session which will remove it from the EOS backend.
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        DestroySessionDelegateHandle =
            Session->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(
                this,
                &ThisClass::HandleDestroySessionCompleted));

        if (!Session->DestroySession(SessionName))
        {
            UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::DestroySession] DestroySession call failed."));
            Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
            DestroySessionDelegateHandle.Reset();
        }
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::DestroySession] Session interface null"));
    }
#endif // !P2PMODE
}

void AEOSGameSession::HandleDestroySessionCompleted(FName EOSSessionName, bool bWasSuccesful)
{
#if !P2PMODE
    // Tutorial 5: This function is triggered via the callback we set in DestroySession once the session is destroyed (or there is a failure).
    if (bWasSuccesful)
    {
        bSessionExists = false; // Mark that the session doesn't exist. This way next time BeginPlay is called a new session will be created.
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleDestroySessionCompleted] Session destroyed."));
    }
    else
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleDestroySessionCompleted] Failed to destroy session (async)."));
    }

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;

    if (Session.IsValid())
    {
        Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
        DestroySessionDelegateHandle.Reset();
    }
#endif // !P2PMODE
}

// Tutorial 11: server-side voice-credentials helper (EOS Voice Web API). Bodies wrapped externally
// in #if !P2PMODE because the helpers are themselves declared #if !P2PMODE in the header (they pull
// in HTTP/JSON/EOSSettings, which we don't carry into P2P builds).

#if !P2PMODE

AEOSPlayerController* AEOSGameSession::FindPlayerControllerByNetId(const FUniqueNetIdRef& PlayerId) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState)
        {
            continue;
        }

        const FUniqueNetIdRepl& StateId = PC->PlayerState->GetUniqueId();
        if (StateId.IsValid() && *StateId == *PlayerId)
        {
            return Cast<AEOSPlayerController>(PC);
        }
    }
    return nullptr;
}

// =====================================================================
// Tutorial 11: Voice on server - per-player room-token issuance.
//
// Server mints per-player EOS RTC room tokens via the EOS Voice Web API
// (OAuth client_credentials -> createRoomToken), then RPCs the token +
// clientBaseUrl to each client via Client_ReceiveVoiceCredentials. The
// client uses those credentials to JoinChannel against the RTC media
// server. Dedicated-server-only - the P2PMODE=1 build uses the OSS
// lobby's auto-managed voice path instead (bUseLobbiesVoiceChatIfAvailable).
// EOSVoiceChat exposes no server-side token-issuance API in 5.8, so we
// hand-roll the OAuth + createRoomToken HTTP calls below.
// =====================================================================

void AEOSGameSession::RequestVoiceCredentialsForPlayer(AEOSPlayerController* TargetPC, const FUniqueNetIdRef& PlayerId)
{
    // Resolve the Server artifact (selected via -epicapp=Server). Reading UEOSSettings from game
    // code is unusual - see the include comment at the top of this file for why it's needed here.
    FEOSArtifactSettings ArtifactSettings;
    if (!UEOSSettings::GetSelectedArtifactSettings(ArtifactSettings))
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer] No selected EOS artifact - cannot call Voice Web API."));
        return;
    }
    if (ArtifactSettings.ClientId.IsEmpty() || ArtifactSettings.ClientSecret.IsEmpty() || ArtifactSettings.DeploymentId.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer] Server artifact is missing credentials - cannot mint voice tokens. Fill in the 'Server' +Artifacts entry in Config/DefaultEngine.ini."));
        return;
    }

    // Voice Web API wants the bare ProductUserId; FUniqueNetId::ToString returns the composite
    // "<EAS>|<PUID>" form. Type-guard the cast: see EOSPlayerController.cpp's identity-check
    if (PlayerId->GetType() != EOS_SUBSYSTEM)
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer] PlayerId is not an EOS id (type=%s)."), *PlayerId->GetType().ToString());
        return;
    }
    const IUniqueNetIdEOS& EosPlayerId = static_cast<const IUniqueNetIdEOS&>(*PlayerId);
    const FString TargetPuid = LexToString(EosPlayerId.GetProductUserId());
    if (TargetPuid.IsEmpty())
    {
        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer] Could not extract ProductUserId from PlayerId=%s"), *OSS_REDACT(PlayerId->ToString()));
        return;
    }

    // Capture per-request state for the async chain. Use a weak ref to the PC so the
    // callback can detect a destroyed controller (WeakPC.Get() returns null) and bail
    // instead of dereferencing freed memory.
    TWeakObjectPtr<AEOSPlayerController> WeakPC(TargetPC);
    const FString LocalRoomName = VoiceRoomName;
    const FString LocalPuid = TargetPuid;
    const FString DeploymentId = ArtifactSettings.DeploymentId;

    // Step 1: OAuth access token (Connect Web API, client_credentials grant). A production
    // title would cache this token and reuse it until expiry (1h) instead of re-authing per player.
    // https://dev.epicgames.com/docs/web-api-ref/connect-web-api#token-request
    const FString BasicAuthRaw = FString::Printf(TEXT("%s:%s"), *ArtifactSettings.ClientId, *ArtifactSettings.ClientSecret);
    const FString BasicAuth = FBase64::Encode(BasicAuthRaw);

    TSharedRef<IHttpRequest> AuthReq = FHttpModule::Get().CreateRequest();
    AuthReq->SetURL(TEXT("https://api.epicgames.dev/auth/v1/oauth/token"));
    AuthReq->SetVerb(TEXT("POST"));
    AuthReq->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Basic %s"), *BasicAuth));
    AuthReq->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    AuthReq->SetHeader(TEXT("Accept"), TEXT("application/json"));
    AuthReq->SetContentAsString(FString::Printf(TEXT("grant_type=client_credentials&deployment_id=%s"), *DeploymentId));

    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer] Requesting OAuth token for Voice Web API (player PUID=%s)."), *OSS_REDACT(LocalPuid));

    AuthReq->OnProcessRequestComplete().BindLambda(
        [WeakPC, LocalRoomName, LocalPuid, DeploymentId]
        (FHttpRequestPtr /*AuthReqPtr*/, FHttpResponsePtr AuthResp, bool bAuthOk)
        {
            if (!bAuthOk || !AuthResp.IsValid() || AuthResp->GetResponseCode() != 200)
            {
                const int32 Code = AuthResp.IsValid() ? AuthResp->GetResponseCode() : -1;
                const FString Body = AuthResp.IsValid() ? AuthResp->GetContentAsString() : TEXT("<no response>");
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer OAuth] OAuth request failed (http=%d): %s"), Code, *Body);
                return;
            }

            // Parse the access_token out of the response body.
            FString AccessToken;
            TSharedPtr<FJsonObject> AuthJson;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AuthResp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, AuthJson) || !AuthJson.IsValid() || !AuthJson->TryGetStringField(TEXT("access_token"), AccessToken) || AccessToken.IsEmpty())
            {
                UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer OAuth] OAuth response missing access_token: %s"), *AuthResp->GetContentAsString());
                return;
            }

            // Step 2: createRoomToken (Voice Web API). Requires `Voice:createRoomToken` policy on the Server client.
            // https://dev.epicgames.com/docs/web-api-ref/voice-web-api#creating-room-tokens
            const FString RoomUrl = FString::Printf(TEXT("https://api.epicgames.dev/rtc/v1/%s/room/%s"), *DeploymentId, *LocalRoomName);
            const FString RoomBody = FString::Printf(
                TEXT("{\"participants\":[{\"puid\":\"%s\",\"hardMuted\":false}]}"),
                *OSS_REDACT(LocalPuid));

            TSharedRef<IHttpRequest> RoomReq = FHttpModule::Get().CreateRequest();
            RoomReq->SetURL(RoomUrl);
            RoomReq->SetVerb(TEXT("POST"));
            RoomReq->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
            RoomReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            RoomReq->SetHeader(TEXT("Accept"), TEXT("application/json"));
            RoomReq->SetContentAsString(RoomBody);

            RoomReq->OnProcessRequestComplete().BindLambda(
                [WeakPC, LocalRoomName, LocalPuid]
                (FHttpRequestPtr /*RoomReqPtr*/, FHttpResponsePtr RoomResp, bool bRoomOk)
                {
                    AEOSPlayerController* PC = WeakPC.Get();
                    if (!PC)
                    {
                        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer createRoomToken] PlayerController went away before voice token arrived - discarding."));
                        return;
                    }

                    if (!bRoomOk || !RoomResp.IsValid() || RoomResp->GetResponseCode() != 200)
                    {
                        const int32 Code = RoomResp.IsValid() ? RoomResp->GetResponseCode() : -1;
                        const FString Body = RoomResp.IsValid() ? RoomResp->GetContentAsString() : TEXT("<no response>");
                        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer createRoomToken] createRoomToken failed (http=%d) for PUID=%s: %s"), Code, *OSS_REDACT(LocalPuid), *Body);
                        return;
                    }

                    TSharedPtr<FJsonObject> RoomJson;
                    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RoomResp->GetContentAsString());
                    if (!FJsonSerializer::Deserialize(Reader, RoomJson) || !RoomJson.IsValid())
                    {
                        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer createRoomToken] createRoomToken response was not valid JSON: %s"), *RoomResp->GetContentAsString());
                        return;
                    }

                    FString ClientBaseUrl;
                    RoomJson->TryGetStringField(TEXT("clientBaseUrl"), ClientBaseUrl);

                    // We requested one PUID, so the token is at participants[0].token.
                    FString ParticipantToken;
                    const TArray<TSharedPtr<FJsonValue>>* ParticipantsArr = nullptr;
                    if (RoomJson->TryGetArrayField(TEXT("participants"), ParticipantsArr) && ParticipantsArr && ParticipantsArr->Num() > 0)
                    {
                        const TSharedPtr<FJsonObject>* FirstObj = nullptr;
                        if ((*ParticipantsArr)[0]->TryGetObject(FirstObj) && FirstObj)
                        {
                            (*FirstObj)->TryGetStringField(TEXT("token"), ParticipantToken);
                        }
                    }

                    if (ClientBaseUrl.IsEmpty() || ParticipantToken.IsEmpty())
                    {
                        UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer createRoomToken] createRoomToken response missing clientBaseUrl or participant token (url empty=%d token empty=%d): %s"), ClientBaseUrl.IsEmpty(), ParticipantToken.IsEmpty(), *RoomResp->GetContentAsString());
                        return;
                    }

                    UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::RequestVoiceCredentialsForPlayer createRoomToken] Forwarding voice credentials for room '%s' to PlayerController %s."), *LocalRoomName, *PC->GetName());
                    PC->Client_ReceiveVoiceCredentials(LocalRoomName, ParticipantToken, ClientBaseUrl);
                });

            RoomReq->ProcessRequest();
        });

    AuthReq->ProcessRequest();
}

#endif // !P2PMODE (covers Tutorial 11 voice helpers - header-conditional declarations)

// =====================================================================
// Tutorial 10: Anti-cheat - server-side glue (dedicated server only).
//
// AC Server interface: RegisterClient (post-VerifyIdToken on the joiner's
// JWT, see EOSPlayerController::Server_NotifyAntiCheatReady), violation
// callbacks routed to KickPlayer. Pairs with the AC client-side wiring
// in EOSPlayerController and the EOSAntiCheat plugin's IEOSAntiCheatServer
// implementation.
// =====================================================================

#if !P2PMODE && ACMODE
void AEOSGameSession::RegisterAntiCheatClient(const FUniqueNetIdRef& PlayerId)
{
    if (!IsRunningDedicatedServer())
    {
        return;
    }

    if (IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get())
    {
        if (IEOSAntiCheatServer* Server = AntiCheat->GetServer())
        {
            Server->RegisterClient(PlayerId);
        }
    }
}

void AEOSGameSession::HandleAntiCheatViolation(const FUniqueNetIdPtr& PlayerId, const FString& Reason)
{
    // PEER_SELF is P2P-only - a null here means a cross-wired bridge.
    if (!PlayerId.IsValid())
    {
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSGameSession::HandleAntiCheatViolation] Null PlayerId - ignoring (unexpected in server mode)."));
        return;
    }
    const FUniqueNetIdRef PlayerRef = PlayerId.ToSharedRef();

    AEOSPlayerController* PC = FindPlayerControllerByNetId(PlayerRef);
    if (!PC)
    {
        UE_LOG(LogEOSOSSTutorial, Warning, TEXT("[AEOSGameSession::HandleAntiCheatViolation] No PlayerController for flagged PlayerId=%s - cannot kick."), *OSS_REDACT(PlayerRef->ToString()));
        return;
    }

    UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSGameSession::HandleAntiCheatViolation] Kicking %s: %s"), *PC->GetName(), *Reason);
    KickPlayer(PC, FText::FromString(Reason));
}

void AEOSGameSession::HandleAntiCheatMessageToClient(const FUniqueNetIdRef& PlayerId, const TArray<uint8>& Bytes)
{
    AEOSPlayerController* PC = FindPlayerControllerByNetId(PlayerId);
    if (!PC)
    {
        // Player may have disconnected between when the SDK queued the message and now.
        return;
    }
    PC->Client_AntiCheatMessage(Bytes);
}
#endif // !P2PMODE && ACMODE
