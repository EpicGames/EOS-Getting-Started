// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOS_Example_AndroidGameMode.h"
#include "EOS_Example_AndroidCharacter.h"
#include "EOSPlayerController.h"
#include "UObject/ConstructorHelpers.h"

AEOS_Example_AndroidGameMode::AEOS_Example_AndroidGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// set default player controller to our Blueprinted player controller
	static ConstructorHelpers::FClassFinder<AEOSPlayerController> PlayerPlayerControllerBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_EOSPlayerController"));
	if (PlayerPlayerControllerBPClass.Class != NULL)
	{
		PlayerControllerClass = PlayerPlayerControllerBPClass.Class;
	}
}
