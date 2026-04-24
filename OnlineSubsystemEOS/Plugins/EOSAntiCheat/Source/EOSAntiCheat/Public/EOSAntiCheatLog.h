// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

/**
 * Log category for this plugin's own output. Separate from the EOS SDK's
 * LogEOSSDK / LogEOSAntiCheat* categories so plugin-level events (BeginSession,
 * Register, kick dispatch, violation delegates) are easy to filter without SDK
 * noise. Bump verbosity with `log LogEOSAntiCheatPlugin Verbose` (or
 * `VeryVerbose` for message byte dumps in non-Shipping builds).
 */
DECLARE_LOG_CATEGORY_EXTERN(LogEOSAntiCheatPlugin, Log, All);
