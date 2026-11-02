// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOS_OSS_TutorialGameMode.h"
#include "EOS_OSS_TutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "EOSPlayerController.h"
#include "EOSGameSession.h"
#include "EOSPlayerState.h"
#if ACMODE && P2PMODE
#include "GameFramework/PlayerState.h"
#include "IEOSAntiCheat.h"
#include "IEOSAntiCheatClient.h"
#endif

AEOS_OSS_TutorialGameMode::AEOS_OSS_TutorialGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	PlayerControllerClass = AEOSPlayerController::StaticClass(); // Tutorial 1: Setting the PlayerController to our custome one.
	// Tutorial 4: We don't need a derived GameSessionClass;
#if !P2PMODE
	GameSessionClass = AEOSGameSession::StaticClass(); // Tutorial 5: Setting the GameSession class to our custom one.
#endif 
	PlayerStateClass = AEOSPlayerState::StaticClass(); // Tutorial 2: Setting the PlayerState class to our custom one. 
	
	// Tutorial 5: In a real game you may want to have a waiting room before sending players to the level. You can use seamless travel to do this and persist the EOS Session across levels. 
	// This is omitted in this course to keep things simple. 
}

#if ACMODE
void AEOS_OSS_TutorialGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Tutorial 10: Super's spawn attempt is refused by the AC gate (CanRestartPlayer). Start the
	// timeout after Super - SetPlayer has run, so IsLocalController() can tell the host apart.
	Super::PostLogin(NewPlayer);

	AEOSPlayerController* EosPC = Cast<AEOSPlayerController>(NewPlayer);
	if (EosPC && EosPC->ShouldGateOnAntiCheat())
	{
		EosPC->StartAntiCheatReadyTimeout();
	}
}
#endif

#if ACMODE && P2PMODE
void AEOS_OSS_TutorialGameMode::Logout(AController* Exiting)
{
	// Tutorial 10: Peer registrations otherwise live as long as lobby membership. Unregistering here
	// ties them to the game connection instead, like UnregisterClient on a dedicated server: a joiner
	// that rejoins (even while still in the lobby) has to reach RemoteAuthComplete again. The joiner
	// drops its side in AEOSPlayerController::EndPlay. HandleParticipantLeft's later UnregisterPeer is a no-op.
	AEOSPlayerController* EosPC = Cast<AEOSPlayerController>(Exiting);
	IEOSAntiCheat* AntiCheat = IEOSAntiCheat::Get();
	IEOSAntiCheatClient* Client = AntiCheat ? AntiCheat->GetClient() : nullptr;
	if (EosPC && EosPC->ShouldGateOnAntiCheat() && Client && EosPC->PlayerState && EosPC->PlayerState->GetUniqueId().IsValid())
	{
		UE_LOG(LogEOSOSSTutorial, Verbose, TEXT("[AEOS_OSS_TutorialGameMode::Logout] %s left the game - unregistering its anti-cheat peer."), *EosPC->GetName());
		Client->UnregisterPeer(EosPC->PlayerState->GetUniqueId()->AsShared());
	}

	Super::Logout(Exiting);
}
#endif
