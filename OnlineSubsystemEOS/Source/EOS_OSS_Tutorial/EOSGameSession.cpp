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


AEOSGameSession::AEOSGameSession()
{
    // Tutorial 3: Including constructor here for clarity. Nothing added in derived class for this tutorial.
}

bool AEOSGameSession::ProcessAutoLogin()
{
    // Tutorial 3: Override base function as players need to login before joining the session. We don't want to call AutoLogin on server.
    return true;
}

FString AEOSGameSession::ApproveLogin(const FString& Options)
{
    if (IsRunningDedicatedServer())
    {
        // If the server is full return an error. Catching the error on the client is NOT implemented in this sample.
        // See UCommonSessionSubsystem::Initialize in the Lyra project for an example.
        Super::ApproveLogin(Options);
        return NumberOfPlayersInSession == MaxNumberOfPlayersInSession ? "FULL" : "";
    }
    else
    {
        return "";
    }
}


void AEOSGameSession::BeginPlay()
{
    // Tutorial 3: Override base function to create session when running as dedicated server.
    Super::BeginPlay();

    if (IsRunningDedicatedServer() && !bSessionExists) // Only create a session if running as a dedicated server and session doesn't exist.
    {
        CreateSession("KeyName", "KeyValue"); // Should parametrize Key/Value pair for custom attribute
    }
}

void AEOSGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Tutorial 3: Override base function to destroy session at end of play. This happens on both dedicated server and client.
    Super::EndPlay(EndPlayReason);
    DestroySession();
}

void AEOSGameSession::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    NumberOfPlayersInSession++; // Keep track of players registered in session

    // Tutorial 3: Add code here if you need to do anything else after a player joins the dedicated server.
}

void AEOSGameSession::NotifyLogout(const APlayerController* ExitingPlayer)
{
    // Tutorial 3: Override base function to handle players leaving EOS Session.
    Super::NotifyLogout(ExitingPlayer); // This calls UnregisterPlayer

    // When players leave the dedicated server we need to check how many players are left. If 0 players are left, session is destroyed.
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
    else
    {
        // This isn't "handling" the error when the server is full, just a log to help keep track of the flow.
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::NotifyLogout] Player is leaving the dedicated server. This may be a kick because the server is full if the player didn't leave intentionally."))
    }
}

void AEOSGameSession::CreateSession(FName KeyName, FString KeyValue) // Dedicated Server Only
{
    // Tutorial 3: This function will create an EOS Session.
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
        SessionSettings->NumPublicConnections = MaxNumberOfPlayersInSession; // We will test our sessions with 2 players to keep things simple.
        SessionSettings->bShouldAdvertise = true; // This creates a public match and will be searchable. This will set the session as joinable via presence.
        SessionSettings->bUsesPresence = false;   // No presence on dedicated server. This requires a local user.
        SessionSettings->bAllowJoinViaPresence = false; // Superset by bShouldAdvertise and will be true on the backend.
        SessionSettings->bAllowJoinViaPresenceFriendsOnly = false; // Superset by bShouldAdvertise and will be true on the backend.
        SessionSettings->bAllowInvites = false;    // Allow inviting players into session. This requires presence and a local user.
        SessionSettings->bAllowJoinInProgress = false; // Once the session is started, no one can join.
        SessionSettings->bIsDedicated = true; // Session created on dedicated server.
        SessionSettings->bUseLobbiesIfAvailable = false; // This is an EOS Session not an EOS Lobby as they aren't supported on Dedicated Servers.
        SessionSettings->bUseLobbiesVoiceChatIfAvailable = false;
        SessionSettings->bUsesStats = true; // Needed to keep track of player stats.

        // This custom attribute will be used in searches on GameClients.
        SessionSettings->Settings.Add(KeyName, FOnlineSessionSetting((KeyValue), EOnlineDataAdvertisementType::ViaOnlineService));

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
}

void AEOSGameSession::HandleCreateSessionCompleted(FName EOSSessionName, bool bWasSuccessful) // Dedicated Server Only
{
    // Tutorial 3: This function is triggered via the callback we set in CreateSession once the session is created (or there is a failure to create).
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
}

void AEOSGameSession::RegisterPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, bool bWasFromInvite)
{
    // Tutorial 3: Override base function to register player in EOS Session.
    Super::RegisterPlayer(NewPlayer, UniqueId, bWasFromInvite);

    if (IsRunningDedicatedServer()) // Only run this on the dedicated server.
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
}

void AEOSGameSession::HandleRegisterPlayerCompleted(FName EOSSessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful)
{
    // Tutorial 3: This function is triggered via the callback we set in RegisterPlayer once the player is registered (or there is a failure).
    if (bWasSuccesful)
    {
        UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSGameSession::HandleRegisterPlayerCompleted] Player registered in EOS session."));
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
}

void AEOSGameSession::UnregisterPlayer(const APlayerController* ExitingPlayer)
{
    // Tutorial 3: Override base function to Unregister player in EOS Session.
    Super::UnregisterPlayer(ExitingPlayer);

    // Only need to unregister the player in the EOS Session on the Server.
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
}

void AEOSGameSession::HandleUnregisterPlayerCompleted(FName EOSSessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful)
{
    // Tutorial 3: This function is triggered via the callback we set in UnregisterPlayer once the player is unregistered (or there is a failure).
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
}

void AEOSGameSession::StartSession()
{
    // Tutorial 3: This function is called once all players are registered. It will mark the EOS Session as started.
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
}

void AEOSGameSession::HandleStartSessionCompleted(FName EOSSessionName, bool bWasSuccessful)
{
    // Tutorial 3: This function is triggered via the callback we set in StartSession once the session is started (or there is a failure).
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
}

void AEOSGameSession::EndSession()
{
    // Tutorial 3: This function is called once all players have left the session. It will mark the EOS Session as ended.
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
}

void AEOSGameSession::HandleEndSessionCompleted(FName EOSSessionName, bool bWasSuccessful)
{
    // Tutorial 3: This function is triggered via the callback we set in EndSession once the session is ended (or there is a failure).
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
}


void AEOSGameSession::DestroySession()
{
    // Tutorial 3: Called when EndPlay() is called. This will destroy the EOS Session which will remove it from the EOS backend.
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
}

void AEOSGameSession::HandleDestroySessionCompleted(FName EOSSessionName, bool bWasSuccesful)
{
    // Tutorial 3: This function is triggered via the callback we set in DestroySession once the session is destroyed (or there is a failure).
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
}
