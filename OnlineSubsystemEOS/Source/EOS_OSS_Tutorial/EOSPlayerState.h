// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "EOSPlayerState.generated.h"

/**
 * 
 */

UCLASS()
class EOS_OSS_TUTORIAL_API AEOSPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	// Class constructor, included for completeness but not used.
	AEOSPlayerState(); 

	// Function to update  player stat - this is used to unlock achievements.
	void UpdateStat(FString StatName, int32 StatValue);

	// Function to return global leaderboard.
	void QueryLeaderboardGlobal(FName LeaderboardName = "JUMPERLEADERBOARD");

	// Function to friends leaderboard based on a single stat.
	void QueryLeaderboardFriends(FString StatName, FName LeaderboardName = "JUMPERLEADERBOARD" );
 
protected:
	// Function called when play begins, included for completeness but not used.
	virtual void BeginPlay();

	// Delegate to bind callback event for when a leaderboard is retrieved. Same delgate used for global and friend leaderboards.
	FDelegateHandle QueryLeaderboardDelegateHandle;

	// Callback function. This function will run when a global OR friend leaderboard is retrieved.
	void HandleQueryLeaderboardComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef LeaderboardReadRef);

	// Callback fired after ReadFriendsList completes. Used by the friend-leaderboard workaround (see QueryLeaderboardFriends).
	void HandleReadFriendsListForLeaderboard(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, FOnlineLeaderboardReadRef LeaderboardReadRef);
};
