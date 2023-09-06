// Fill out your copyright notice in the Description page of Project Settings.

#include "EOSPlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"

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
	GlobalLeaderboardReadRef->LeaderboardName = FName(LeaderboardName);
	
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

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();

	// Check if player is logged in... 
	FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);
	
	if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
	{
		return;
	}

	// Prepare arguments. Notice the column metadata is a stat and can be different than the sorted column
	FOnlineLeaderboardReadRef FriendLeaderboardReadRef = MakeShared<FOnlineLeaderboardRead, ESPMode::ThreadSafe>();
	FriendLeaderboardReadRef->LeaderboardName = FName(LeaderboardName);
	FriendLeaderboardReadRef->ColumnMetadata.Add(FColumnMetaData(FName(StatName),EOnlineKeyValuePairDataType::Int32)); 
	FriendLeaderboardReadRef->SortedColumn = FName(StatName);

	// Create a delegate handle and pass in the function to execute once the leaderboard is fetch. 
	// The function here is the same as with the global leaderboard. 
	QueryLeaderboardDelegateHandle =
		Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
			this,
			&ThisClass::HandleQueryLeaderboarComplete,
			FriendLeaderboardReadRef));

	// Try to read the leaderboard. If it fails, log the error, clear and reset the delegate. 
	if (!Leaderboards->ReadLeaderboardsForFriends(0, FriendLeaderboardReadRef))
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




