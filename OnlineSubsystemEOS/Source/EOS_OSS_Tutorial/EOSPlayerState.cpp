// Fill out your copyright notice in the Description page of Project Settings.

#include "EOSPlayerState.h"
#include "EOSPlayerController.h" // For LogEOSOSSTutorial category
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "OnlineKeyValuePair.h" // FVariantData (FOnlineAchievementsWrite::Properties values).
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineSessionInterface.h"  // OnRep_UniqueId / EndPlay sync local OSS RegisteredPlayers.

#if !P2PMODE
// Match EOSPlayerController::JoinSession ('SessionName' literal) and
// AEOSGameSession::SessionName so the OSS named-session key on the
// PlayerState side lines up with what the controller and game-session
// already created on this peer.
static const FName ActiveSessionName = TEXT("SessionName");
#endif

// All the code in here is for tutorial 5: Stats, Achievements and Leaderboards

AEOSPlayerState::AEOSPlayerState()
{
	// Including class constructor for completeness
}

void AEOSPlayerState::BeginPlay()
{
	// Including BeginPlay() for completeness
	Super::BeginPlay();
}

void AEOSPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();

#if !P2PMODE
	// Fires on clients each time a PlayerState's UniqueId arrives via
	// replication (server's local roster is already populated by
	// AGameSession::RegisterPlayer in PostLogin). For each PlayerState we
	// receive - including our own - mirror the engine event into the local
	// OSS named session so the EGS social overlay and friend-presence flows
	// see a complete roster.
	FUniqueNetIdPtr Id = GetUniqueId().GetUniqueNetId();
	if (!Id.IsValid())
	{
		return;
	}

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
	if (!Session.IsValid() || !Session->GetNamedSession(ActiveSessionName))
	{
		return;
	}

	// OSS-EOS dedupes on RegisteredPlayers (OnlineSessionEOS.cpp:3497) so
	// duplicate calls are safe (one OSS Log line on dedup, no backend hit).
	UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerState::OnRep_UniqueId] RegisterPlayer %s in session '%s'."),
		*OSS_REDACT(Id->ToDebugString()), *ActiveSessionName.ToString());
	Session->RegisterPlayer(ActiveSessionName, *Id, /*bWasInvited=*/ false);
#endif
}

void AEOSPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !P2PMODE
	// Mirror OnRep_UniqueId on the way out - clients only. Server-side
	// unregister is handled by AGameSession::NotifyLogout already.
	if (GetNetMode() == NM_Client)
	{
		FUniqueNetIdPtr Id = GetUniqueId().GetUniqueNetId();
		if (Id.IsValid())
		{
			IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
			IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
			if (Session.IsValid() && Session->GetNamedSession(ActiveSessionName))
			{
				UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerState::EndPlay] UnregisterPlayer %s from session '%s'."),
					*OSS_REDACT(Id->ToDebugString()), *ActiveSessionName.ToString());
				Session->UnregisterPlayer(ActiveSessionName, *Id);
			}
		}
	}
#endif
	Super::EndPlay(EndPlayReason);
}

// =====================================================================
// Tutorial 2: Achievements, stats, and leaderboards.
//
// Four OSS entry points exercised here:
//   - IOnlineStats::UpdateStats: client posts stat deltas to the EOS
//     backend (e.g. JumpCount++). The EOS achievement system listens
//     to stat thresholds and unlocks achievements automatically once
//     thresholds are crossed - no separate unlock call needed.
//   - IOnlineAchievements::WriteAchievements: direct unlock by
//     achievement ID. Use this for achievements that aren't backed by
//     a measurable stat (story beats, one-off triggers, hidden ones).
//     OSS-EOS dispatches this straight to EOS_Achievements_UnlockAchievements
//     and only reads the *keys* of WriteObject->Properties as IDs - the
//     stored FVariantData values are ignored.
//   - IOnlineLeaderboards::ReadLeaderboardsAroundRank: global leaderboard
//     query, returns top-N or around-local-rank rows.
//   - IOnlineLeaderboards::ReadLeaderboards (with friend NetId list):
//     friend-scoped leaderboard. The two-step ReadFriendsList -> build
//     id array -> ReadLeaderboards flow works around an OSS-EOS crash
//     in 5.8 ReadLeaderboardsForFriends - see the workaround note above
//     QueryLeaderboardFriends below for the underlying assert.
// Mode-agnostic. Stats post from UpdateStat (called by gameplay); direct
// unlocks and leaderboard queries surface via Test* exec commands.
// =====================================================================

void AEOSPlayerState::UpdateStat(FString StatName, int32 StatValue)
{
	// This function will add a StatValue to the StatName on the EOS backend.
	// If the achievement stat threshold is met, the achievement will unlock.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineStatsPtr Stats = Subsystem ? Subsystem->GetStatsInterface() : nullptr;

	if (Identity.IsValid() && Stats.IsValid())
	{
		// Check if player is logged in before trying to update stat.
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			// Prepare stat update by setting input arguments to update stat.
			FOnlineStatsUserUpdatedStats StatToUpdate = FOnlineStatsUserUpdatedStats(LocalNetId.ToSharedRef());
			FOnlineStatUpdate IngestAmount = FOnlineStatUpdate(StatValue, FOnlineStatUpdate::EOnlineStatModificationType::Unknown);
			StatToUpdate.Stats.Add(StatName.ToUpper(), IngestAmount);

			TArray<FOnlineStatsUserUpdatedStats> StatsToUpdate;
			StatsToUpdate.Add(StatToUpdate);

			// Unlike other OSS functions we've seen in previous modules, there is no delegate handle for Stat Updates.
			// Instead we use an inline weak lambda so we don't keep `this` alive past actor destruction.
			Stats->UpdateStats(LocalNetId.ToSharedRef(), StatsToUpdate,
				FOnlineStatsUpdateStatsComplete::CreateWeakLambda(this, [](const FOnlineError& UpdateResult)
					{
						if (!UpdateResult.bSucceeded)
						{
							UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UpdateStat] Error updating player statistics: %s"), *UpdateResult.ErrorCode);
						}
					}));

			// Fetch our 2 leaderboards when updating stats.
			QueryLeaderboardGlobal();
			QueryLeaderboardFriends(StatName);
		}
		else
		{
			UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UpdateStat] Failed - Player logged out"));
		}
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UpdateStat] Identity and/or Stats interface null"));
	}
}

void AEOSPlayerState::UnlockAchievement(FString AchievementId)
{
	// This function will directly unlock the named EOS achievement for the
	// local user (no stat threshold required). Pair with QueryAchievements
	// in real games to refresh the local cache after the unlock lands.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineAchievementsPtr Achievements = Subsystem ? Subsystem->GetAchievementsInterface() : nullptr;

	if (Identity.IsValid() && Achievements.IsValid())
	{
		// Check if player is logged in before trying to unlock.
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			if (AchievementId.IsEmpty())
			{
				UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UnlockAchievement] AchievementId is empty"));
				return;
			}

			// Build the write object. OSS-EOS only inspects the property keys (achievement IDs);
			// the stored FVariantData value is unused. Multiple keys could be added here to
			// unlock several achievements in one call.
			FOnlineAchievementsWriteRef WriteObject = MakeShared<FOnlineAchievementsWrite, ESPMode::ThreadSafe>();
			WriteObject->Properties.Add(AchievementId, FVariantData());

			Achievements->WriteAchievements(*LocalNetId, WriteObject,
				FOnAchievementsWrittenDelegate::CreateUObject(this, &ThisClass::HandleUnlockAchievementComplete));
		}
		else
		{
			UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UnlockAchievement] Failed - Player logged out"));
		}
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::UnlockAchievement] Identity and/or Achievements interface null"));
	}
}

void AEOSPlayerState::HandleUnlockAchievementComplete(const FUniqueNetId& UserId, const bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogEOSOSSTutorial, Log, TEXT("[AEOSPlayerState::HandleUnlockAchievementComplete] Achievement unlock succeeded for %s."),
			*OSS_REDACT(UserId.ToDebugString()));
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::HandleUnlockAchievementComplete] Achievement unlock failed for %s."),
			*OSS_REDACT(UserId.ToDebugString()));
	}
}

void AEOSPlayerState::QueryLeaderboardGlobal(FName LeaderboardName)
{
	// This function will retrieve a global leaderboard for a certain rank range.
	// The rank range is hardcoded to 0,10. In a real game you may want to pass this as a parameter.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineLeaderboardsPtr Leaderboards = Subsystem ? Subsystem->GetLeaderboardsInterface() : nullptr;

	if (Identity.IsValid() && Leaderboards.IsValid())
	{
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			FOnlineLeaderboardReadRef GlobalLeaderboardReadRef = MakeShared<FOnlineLeaderboardRead, ESPMode::ThreadSafe>();
			// UE 5.8: FOnlineLeaderboardRead::LeaderboardName / SortedColumn are FNameDeprecationWrapper (FString-based).
			GlobalLeaderboardReadRef->LeaderboardName = LeaderboardName.ToString();

			// Bind the shared completion handler (used for both global and friend leaderboards).
			QueryLeaderboardDelegateHandle =
				Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
					this,
					&ThisClass::HandleQueryLeaderboardComplete,
					GlobalLeaderboardReadRef));

			// Try to read the leaderboard. If it fails synchronously, log, clear and reset the delegate.
			if (!Leaderboards->ReadLeaderboardsAroundRank(0, 10, GlobalLeaderboardReadRef))
			{
				UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::QueryLeaderboardGlobal] ReadLeaderboardsAroundRank call failed."));
				Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
				QueryLeaderboardDelegateHandle.Reset();
			}
		}
		else
		{
			UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::QueryLeaderboardGlobal] Failed - Player logged out"));
		}
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::QueryLeaderboardGlobal] Identity and/or Leaderboards interface null"));
	}
}

// Make sure in your code you don't call this too early. The EOS OSS needs to populate the friend list shortly after login.
void AEOSPlayerState::QueryLeaderboardFriends(FString StatName, FName LeaderboardName)
{
	// This function will retrieve a friend leaderboard with specific columns and a sorted column.
	// For this course we are using a single Stat.
	//
	// UE 5.8 WORKAROUND: The obvious one-call path here would be IOnlineLeaderboards::ReadLeaderboardsForFriends().
	// However, in the UE 5.8 EOS OSS plugin that implementation asserts/crashes:
	//   Engine/Plugins/Online/OnlineSubsystemEOS/.../OnlineLeaderboardsEOS.cpp:327 calls
	//       EOSSubsystem->UserManager->GetFriendsList(LocalUserNum, FString(), Friends);
	//   passing an empty friends-list name. GetFriendsList() then calls
	//       EFriendsLists::FromString(Type, TEXT("")) in OnlineFriendsInterface.h, which since 5.8
	//       ends its fallthrough branch with checkNoEntry() -> Assertion failed crash.
	// The fix upstream would be to pass EFriendsLists::ToString(EFriendsLists::Default) instead of
	// FString(). Once a UE hotfix or future release ships that resolves it, this workaround can
	// be replaced with a single ReadLeaderboardsForFriends() call again.
	//
	// Workaround: explicitly ReadFriendsList("default") first, then build the friend-id array and
	// call the lower-level ReadLeaderboards() directly. This also makes the underlying flow more
	// obvious for tutorial readers.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;

	if (Identity.IsValid() && Friends.IsValid())
	{
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			// Prepare arguments. Notice the column metadata is a stat and can be different than the sorted column.
			// UE 5.8: FOnlineLeaderboardRead::LeaderboardName / SortedColumn are FNameDeprecationWrapper (FString-based); FColumnMetaData takes FString.
			FOnlineLeaderboardReadRef FriendLeaderboardReadRef = MakeShared<FOnlineLeaderboardRead, ESPMode::ThreadSafe>();
			FriendLeaderboardReadRef->LeaderboardName = LeaderboardName.ToString();
			FriendLeaderboardReadRef->ColumnMetadata.Add(FColumnMetaData(StatName, EOnlineKeyValuePairDataType::Int32));
			FriendLeaderboardReadRef->SortedColumn = StatName;

			// Step 1: Populate the local friends list. The list name must match one of EFriendsLists values
			// (default / onlinePlayers / inGamePlayers / inGameAndSessionPlayers). We use Default here.
			const FString DefaultList = EFriendsLists::ToString(EFriendsLists::Default);
			Friends->ReadFriendsList(0, DefaultList, FOnReadFriendsListComplete::CreateUObject(
				this,
				&ThisClass::HandleReadFriendsListForLeaderboard,
				FriendLeaderboardReadRef));
		}
		else
		{
			UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::QueryLeaderboardFriends] Failed - Player logged out"));
		}
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::QueryLeaderboardFriends] Identity and/or Friends interface null"));
	}
}

void AEOSPlayerState::HandleReadFriendsListForLeaderboard(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, FOnlineLeaderboardReadRef LeaderboardReadRef)
{
	// Callback of the ReadFriendsList() call in QueryLeaderboardFriends. See the note there for why
	// the tutorial manually resolves friend IDs instead of calling ReadLeaderboardsForFriends().
	if (!bWasSuccessful)
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::HandleReadFriendsListForLeaderboard] Could not read friends list for leaderboard query: %s"), *ErrorStr);
		return;
	}

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
	IOnlineLeaderboardsPtr Leaderboards = Subsystem ? Subsystem->GetLeaderboardsInterface() : nullptr;

	if (Friends.IsValid() && Leaderboards.IsValid())
	{
		// Step 2: Read the now-cached friends list and build an array of unique IDs.
		TArray<TSharedRef<FOnlineFriend>> FriendsArray;
		Friends->GetFriendsList(LocalUserNum, ListName, FriendsArray);

		if (FriendsArray.Num() == 0)
		{
			UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOSPlayerState::HandleReadFriendsListForLeaderboard] No friends in list - skipping friend leaderboard query."));
			return;
		}

		TArray<FUniqueNetIdRef> FriendIds;
		FriendIds.Reserve(FriendsArray.Num());
		for (const TSharedRef<FOnlineFriend>& Friend : FriendsArray)
		{
			FriendIds.Add(Friend->GetUserId());
		}

		// Step 3: Bind the leaderboard-read callback and issue the explicit ReadLeaderboards() call.
		// Same handler used for the global leaderboard path.
		QueryLeaderboardDelegateHandle =
			Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
				this,
				&ThisClass::HandleQueryLeaderboardComplete,
				LeaderboardReadRef));

		if (!Leaderboards->ReadLeaderboards(FriendIds, LeaderboardReadRef))
		{
			UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::HandleReadFriendsListForLeaderboard] ReadLeaderboards call failed."));
			Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
			QueryLeaderboardDelegateHandle.Reset();
		}
	}
	else
	{
		UE_LOG(LogEOSOSSTutorial, Error, TEXT("[AEOSPlayerState::HandleReadFriendsListForLeaderboard] Friends and/or Leaderboards interface null"));
	}
}

void AEOSPlayerState::HandleQueryLeaderboardComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef GlobalLeaderboardReadRef)
{
	// Function triggered when either global or friend leaderboard query completes.
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineLeaderboardsPtr Leaderboards = Subsystem ? Subsystem->GetLeaderboardsInterface() : nullptr;

	if (bWasSuccessful)
	{
		// In a real game you would store the leaderboard data and show it in a UI.
		// To keep things simple in this course, we are writing the data to the UE logs.
		for (const auto& Row : GlobalLeaderboardReadRef->Rows)
		{
			// Partial results from EOS can carry invalid PlayerId entries - guard before dereference.
			if (Row.PlayerId.IsValid())
			{
				UE_LOG(LogEOSOSSTutorial, VeryVerbose, TEXT("[AEOSPlayerState::HandleQueryLeaderboardComplete] Player Id: %s, Rank: %d"), *OSS_REDACT(Row.PlayerId->ToString()), Row.Rank);
			}
		}
	}

	if (Leaderboards.IsValid())
	{
		Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
		QueryLeaderboardDelegateHandle.Reset();
	}
}
