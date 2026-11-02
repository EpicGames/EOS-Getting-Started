// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EOS_OSS_TutorialGameMode.generated.h"

UCLASS(minimalapi)
class AEOS_OSS_TutorialGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEOS_OSS_TutorialGameMode();

#if ACMODE
	// Tutorial 10: Starts the anti-cheat ready timeout for gated joiners.
	virtual void PostLogin(APlayerController* NewPlayer) override;
#endif

#if ACMODE && P2PMODE
	// Tutorial 10: P2P host drops a joiner's anti-cheat peer registration when its game connection ends.
	virtual void Logout(AController* Exiting) override;
#endif

protected:
};



