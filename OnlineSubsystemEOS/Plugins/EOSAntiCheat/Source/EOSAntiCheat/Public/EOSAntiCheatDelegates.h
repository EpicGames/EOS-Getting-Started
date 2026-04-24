// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

/**
 * Broadcast when the server-side AntiCheat interface reports a player has been
 * flagged and kicked. Subscribers can surface UI, record telemetry, or trigger
 * additional game-level responses. Runs on the game thread.
 *
 * Parameters:
 *   - PlayerId:   the offending player's FUniqueNetIdRef (EOS ProductUserId wrapped).
 *   - Reason:     short human-readable string from the SDK's action/status enum.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAntiCheatViolation, const FUniqueNetIdRef& /*PlayerId*/, const FString& /*Reason*/);
