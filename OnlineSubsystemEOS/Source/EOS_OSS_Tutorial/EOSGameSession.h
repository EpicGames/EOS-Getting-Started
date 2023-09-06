// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "EOSGameSession.generated.h"

/**
 * 
 */
UCLASS()
class EOS_OSS_TUTORIAL_API AEOSGameSession : public AGameSession
{
	GENERATED_BODY()
	
protected:
	// Class constructor. We won't use this in this tutorial. 
	AEOSGameSession();

	// Function called when play begins.
	virtual void BeginPlay();

	//Function called when play ends. We are using this to destroy session. 
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	// This function exists on the base class AGameSession. We want to override it so we don't try to login from a dedicated server. 
	virtual bool ProcessAutoLogin();

	// Function called when players join dedicated server. Not used in this tutorial. 
	virtual void PostLogin(APlayerController* NewPlayer);

	// Function called when players leave the dedicated server. Trigger UnregisterPlayer from base class and used to End Sesion. 
	virtual void NotifyLogout(const APlayerController* ExitingPlayer);

	// Hardcoding the session name for this tutorial. 
	FName SessionName = "SessionName"; 

	// Hardcoding the max number of players in a session. 
	const int MaxNumberOfPlayersInSession = 2;

	// Variable to keep track of the number of players in a session.  
	int NumberOfPlayersInSession;

	// Function to create an EOS session. 
	void CreateSession(FName KeyName = "KeyName", FString KeyValue= "KeyValue");

	// Callback function. This function will run when creating the session compeletes. 
	void HandleCreateSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for session creation. 
	FDelegateHandle CreateSessionDelegateHandle;

	// Used to keep track if the session exists or not. 
	bool bSessionExists = false;

	// Function to register our players in the EOS Session. 
	virtual void RegisterPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, bool bWasFromInvite=false);

	// Callback function. This function will run when registering the player compeletes. 
	void HandleRegisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate to bind callback event for register player. 
	FDelegateHandle RegisterPlayerDelegateHandle;

	// Function to unregister our players in the EOS Session.
	virtual void UnregisterPlayer(const APlayerController* ExitingPlayer);
	
	// Callback function. This function will run when unregistering the player compeletes.
	void HandleUnregisterPlayerCompleted(FName SessionName, const TArray<FUniqueNetIdRef>& PlayerIds, bool bWasSuccesful);

	// Delegate to bind callback event for unregister player. 
	FDelegateHandle UnregisterPlayerDelegateHandle;

	// Function to start EOS Session. 
	void StartSession(); 

	// Callback function. This function will run when start session compeletes.
	void HandleStartSessionCompleted(FName SessionName, bool bWasSuccessful); 

	// Delegate to bind callback event for start session. 
	FDelegateHandle StartSessionDelegateHandle;

	// Function to end EOS Session. 
	void EndSession();

	// Callback function. This function will run when end session compeletes.
	void HandleEndSessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for end session. 
	FDelegateHandle EndSessionDelegateHandle;

	// Function to Destroy EOS Session. 
	void DestroySession(); 

	// Callback function. This function will run when destroy session compeletes.
	void HandleDestroySessionCompleted(FName SessionName, bool bWasSuccessful);

	// Delegate to bind callback event for destroy session. 
	FDelegateHandle DestroySessionDelegateHandle; 
};
