// Fill out your copyright notice in the Description page of Project Settings.

#include "EOSPlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"

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

void AEOSPlayerState::UpdateStat(FString StatName, int32 StatValue)
{
	// This function will add a StatValue to the StatName on the EOS backend.
	// If the achievement stat threshold is meant, the achievement will unlock
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	IOnlineStatsPtr Stats = Subsystem->GetStatsInterface();

	// Check if player is online before trying to update stat 
	FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

	if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
	{
		return;
	}
	
	// Prepare stat update by setting input arguments to update stat. 
	FOnlineStatsUserUpdatedStats StatToUpdate = FOnlineStatsUserUpdatedStats(NetId.ToSharedRef());
	FOnlineStatUpdate IngestAmount = FOnlineStatUpdate(StatValue, FOnlineStatUpdate::EOnlineStatModificationType::Unknown); 
	StatToUpdate.Stats.Add(StatName.ToUpper(), IngestAmount); 

	TArray<FOnlineStatsUserUpdatedStats> StatsToUpdate; 
	StatsToUpdate.Add(StatToUpdate); 

	// Unlike other OSS functions we've seen in previous modules, there is no delegate handle for Stat Updates. 
	// Instead we will use an inline lambda 
	Stats->UpdateStats(NetId.ToSharedRef(),StatsToUpdate,
		FOnlineStatsUpdateStatsComplete::CreateLambda([](
			const FOnlineError& UpdateResult)
			{
				// Just log if the update failed. 
				if (!UpdateResult.bSucceeded)
				{
					UE_LOG(LogTemp, Warning, TEXT("Error updating player statistics: %s"), *UpdateResult.ErrorCode);
					return;
				}
			}));
	
	// Fetch our 2 leaderboards when updating stats. 
	QueryLeaderboardGlobal(); 
	QueryLeaderboardFriends(StatName); 
}

void AEOSPlayerState::QueryLeaderboardGlobal(FName LeaderboardName)
{
	// This function will retrieve a global leaderboard for a certain rank range
	// The rank range is hardcoded to 0,10. In a real game you may want to pass this as a parameter. 

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();

	// Again check if the player is logged in 
	FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

	if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
	{
		return;
	}

	FOnlineLeaderboardReadRef GlobalLeaderboardReadRef = MakeShared<FOnlineLeaderboardRead, ESPMode::ThreadSafe>();
	// UE 5.8: FOnlineLeaderboardRead::LeaderboardName / SortedColumn are now FNameDeprecationWrapper (an FString-based wrapper). Assign an FString.
	GlobalLeaderboardReadRef->LeaderboardName = LeaderboardName.ToString();
	
	// Create a delegate handle and pass in the function to execute once the leaderboard is fetch. 
	// The function here is the same as with the friends leaderboard. 
	QueryLeaderboardDelegateHandle = 
		Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
		this,
		&ThisClass::HandleQueryLeaderboarComplete,
		GlobalLeaderboardReadRef));

	// Try to read the leaderboard. If it fails, log the error, clear and reset the delegate. 
	if (!Leaderboards->ReadLeaderboardsAroundRank(0,10, GlobalLeaderboardReadRef))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read global leaderboard.")); 
		Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
		QueryLeaderboardDelegateHandle.Reset();
	}
}

// Make sure in your code you don't call this too early. The EOS OSS needs to populate friend list shortly after login.
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
	// FString(), but as of the ++Fortnite+Main CL we checked the bug is still present - so it has
	// not yet been fixed in UE5-Main. Once a 5.8 hotfix or 5.9 ships that resolves it, this
	// workaround can be replaced with a single ReadLeaderboardsForFriends() call again.
	//
	// Workaround: explicitly ReadFriendsList("default") first, then build the friend-id array and
	// call the lower-level ReadLeaderboards() directly. This also makes the underlying flow more
	// obvious for tutorial readers.

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	IOnlineFriendsPtr Friends = Subsystem->GetFriendsInterface();

	// Check if player is logged in...
	FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);

	if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
	{
		return;
	}

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

void AEOSPlayerState::HandleReadFriendsListForLeaderboard(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, FOnlineLeaderboardReadRef LeaderboardReadRef)
{
	// Callback of the ReadFriendsList() call in QueryLeaderboardFriends. See the note there for why
	// the tutorial manually resolves friend IDs instead of calling ReadLeaderboardsForFriends().

	if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not read friends list for leaderboard query: %s"), *ErrorStr);
		return;
	}

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineFriendsPtr Friends = Subsystem->GetFriendsInterface();
	IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();

	// Step 2: Read the now-cached friends list and build an array of unique IDs.
	TArray<TSharedRef<FOnlineFriend>> FriendsArray;
	Friends->GetFriendsList(LocalUserNum, ListName, FriendsArray);

	if (FriendsArray.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No friends in list - skipping friend leaderboard query."));
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
			&ThisClass::HandleQueryLeaderboarComplete,
			LeaderboardReadRef));

	if (!Leaderboards->ReadLeaderboards(FriendIds, LeaderboardReadRef))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read friend leaderboard."));
		Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
		QueryLeaderboardDelegateHandle.Reset();
	}
}

void AEOSPlayerState::HandleQueryLeaderboarComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef GlobalLeaderboardReadRef)
{
	// Function triggered when either global or friend leaderboard query completes. 
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();

	if (bWasSuccessful)
	{
		// In a real game you would store the leaderboard data and show it in a UI. 
		// To keep things simple in this course, we are writing the data to the UE logs. 
		for ( auto Row : GlobalLeaderboardReadRef->Rows )
		{
			UE_LOG(LogTemp, Log, TEXT("Player Id: %s, Player Rank: %d"), *(*Row.PlayerId).ToString(), Row.Rank);
		}
	}

	Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
	QueryLeaderboardDelegateHandle.Reset();
}




