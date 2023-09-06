// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOS_OSS_TutorialGameMode.h"
#include "EOS_OSS_TutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "EOSPlayerController.h"
#include "EOSGameSession.h"
#include "EOSPlayerState.h"

AEOS_OSS_TutorialGameMode::AEOS_OSS_TutorialGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	PlayerControllerClass = AEOSPlayerController::StaticClass(); // Tutorial 2: Setting the PlayerController to our custome one.
	// Tutorial 7: We don't need a derived GameSessionClass;
#if !P2PMODE
	GameSessionClass = AEOSGameSession::StaticClass(); // Tutorial 3: Setting the GameSession class to our custom one.
#endif 
	PlayerStateClass = AEOSPlayerState::StaticClass(); // Tutorial 5: Setting the PlayerState class to our custom one. 
	
	// Tutorial 3: In a real game you may want to have a waiting room before sending players to the level. You can use seamless travel to do this and persist the EOS Session across levels. 
	// This is omitted in this course to keep things simple. 
}


