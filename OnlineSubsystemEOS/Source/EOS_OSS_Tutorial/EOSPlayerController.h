// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h" //Don't like declaring this here but getting weird compiler error with EOnJoinSessionCompleteResult
#include "EOSPlayerController.generated.h"


/**
 * Child class of APlayerController to hold EOS OSS code. 
 */

 //Need to forward declare classes used 
class FOnlineSessionSearch;
class FOnlineSessionSearchResult;

UCLASS()
class EOS_OSS_TUTORIAL_API AEOSPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	// Class constructor. We won't use this in this tutorial. 
	AEOSPlayerController();

protected:
	// Function called when play begins
	virtual void BeginPlay();

	//Function to sign into EOS Game Services
	void Login();

	//Callback function. This function is ran when signing into EOS Game Services completes. 
	void HandleLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	//Delegate to bind callback event for login. 
	FDelegateHandle LoginDelegateHandle;

	// Function to find EOS sessions. Hardcoded attribute key/value pair to keep things simple
	void FindSessions(FName SearchKey = "KeyName", FString SearchValue = "KeyValue");

	// Callback function. This function will run when the session is found.
	void HandleFindSessionsCompleted(bool bWasSuccessful, TSharedRef<FOnlineSessionSearch> Search);

	//Delegate to bind callback event for when sessions are found.
	FDelegateHandle FindSessionsDelegateHandle;

	FString ConnectString;

	FOnlineSessionSearchResult* SessionToJoin; 

	void JoinSession();

	void HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	FDelegateHandle JoinSessionDelegateHandle;

};
