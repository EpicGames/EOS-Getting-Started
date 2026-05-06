// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineLeaderboardInterface.h" // Tutorial 2: FOnlineLeaderboardReadRef in HandleQueryLeaderboardComplete signature.
#include "Interfaces/OnlineFriendsInterface.h"     // Tutorial 2: friend-list types used by the friend-leaderboard workaround.
#include "EOSPlayerState.generated.h"

/**
 * Child class of APlayerState to hold stats, achievements, and leaderboards.
 */
UCLASS()
class EOS_OSS_TUTORIAL_API AEOSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// Class constructor. We won't use this in this tutorial.
	AEOSPlayerState();

	/* =============== Tutorial 2 - Achievements + Stats + Leaderboards ============================= */

	// Function to directly unlock an achievement by ID. EOS supports two unlock paths: stat-threshold (UpdateStat below) and direct (this function). Use direct for achievements that aren't tied to a measurable stat.
	void UnlockAchievement(FString AchievementId);

	// Function to update a player stat. Stat thresholds can drive achievement unlocks.
	void UpdateStat(FString StatName, int32 StatValue);

	// Function to query the global leaderboard.
	void QueryLeaderboardGlobal(FName LeaderboardName = "JUMPERLEADERBOARD");

	// Function to query the friend-scoped leaderboard for a single stat.
	void QueryLeaderboardFriends(FString StatName, FName LeaderboardName = "JUMPERLEADERBOARD");

protected:

	// Function called when play begins.
	virtual void BeginPlay();

	// Callback function. Ran when a global OR friend leaderboard query completes (same delegate is reused for both).
	void HandleQueryLeaderboardComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef LeaderboardReadRef);

	// Delegate handle for leaderboard reads. Same handle used for global and friend leaderboards.
	FDelegateHandle QueryLeaderboardDelegateHandle;

	// Callback function. Ran when ReadFriendsList completes during the friend-leaderboard flow (see QueryLeaderboardFriends for why the read is split in two).
	void HandleReadFriendsListForLeaderboard(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, FOnlineLeaderboardReadRef LeaderboardReadRef);

	// Callback function. Ran when UnlockAchievement (WriteAchievements) completes. Parameter is named UserId rather than PlayerId to avoid shadowing APlayerState::PlayerId.
	void HandleUnlockAchievementComplete(const FUniqueNetId& UserId, const bool bWasSuccessful);

	// Function called when UniqueId replicates in. Calls Session->RegisterPlayer locally so the OSS named session has a complete roster on each client (server-mode workaround - see .cpp for full rationale).
	virtual void OnRep_UniqueId() override;

	// Function called when play ends. Mirrors OnRep_UniqueId on the way out - clients only.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
