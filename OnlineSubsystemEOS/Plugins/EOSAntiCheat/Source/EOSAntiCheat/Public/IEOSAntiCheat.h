// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Online/CoreOnline.h"

// Forward declarations
#if !P2PMODE
class IEOSAntiCheatServer;
#endif
class IEOSAntiCheatClient;

/** New log category for this plugin. Bump verbosity with `log LogEOSAntiCheatPlugin Verbose` (or `VeryVerbose` for message byte dumps in non-Shipping builds). Separate from the EOS SDK's LogEOSSDK / LogEOSAntiCheat* categories so plugin-level events (BeginSession, Register, kick dispatch, violation delegates) are easy to filter without SDK noise. */
DECLARE_LOG_CATEGORY_EXTERN(LogEOSAntiCheatPlugin, Log, All);

/** Unified violation signal for both modes. Game-thread. PlayerId is null when the LOCAL client was flagged (PEER_SELF in P2P). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAntiCheatViolation, const FUniqueNetIdPtr& /*PlayerId, null = self*/, const FString& /*Reason*/);

/* =============== Tutorial 10 - Anti-Cheat (module-level singleton) ============================= */

/**
 * Module-level singleton that owns the plugin's server-side and client-side
 * EOS AntiCheat wrappers. Modeled after IOnlineSubsystem: one entry point,
 * accessor-per-interface. Callers reach the plugin as:
 *
 *   if (IEOSAntiCheat* AC = IEOSAntiCheat::Get())
 *   {
 *       AC->GetServer()->BeginSession(...);
 *   }
 *
 * Server and client accessors may return null on platforms / build
 * configurations where the corresponding interface isn't applicable (e.g. the
 * server interface in a pure-client shipping build, or both in an Editor
 * build - the plugin no-ops in the editor per the integration contract).
 *
 * Threading: the EOS SDK is NOT thread-safe. Every SDK call and every SDK
 * callback runs on the thread that ticks EOS_Platform_Tick, which
 * OnlineSubsystemEOS pumps from the game thread. This plugin inherits that
 * contract: every public delegate below fires on the game thread, and every
 * plugin entry point must be called from the game thread. Do not move plugin
 * work to a worker thread or stand up a second EOS_Platform_Tick - either
 * will corrupt SDK state.
 */
class EOSANTICHEAT_API IEOSAntiCheat : public IModuleInterface
{
public:
	// Function to get the singleton, or nullptr if the module isn't loaded. Safe to call from gameplay code; internally just a LoadModulePtr lookup.
	static IEOSAntiCheat* Get();

#if !P2PMODE
	// Function to get the server-side wrapper, or null if this build / configuration can't host one.
	virtual IEOSAntiCheatServer* GetServer() = 0;
#endif

	// Function to get the client-side wrapper, or null if this build / configuration can't host one.
	virtual IEOSAntiCheatClient* GetClient() = 0;

	// Multicast accessor. Fires on the game thread when the server-side AntiCheat interface flags and kicks a player. See FOnAntiCheatViolation above for the payload.
	virtual FOnAntiCheatViolation& OnViolation() = 0;

	// Function to copy a signed JWT for the local user. The client hands this to the server so the server can call VerifyIdToken before trusting the claimed PUID. Returns false if the local user isn't Connect-logged-in yet.
	virtual bool CopyLocalIdToken(const FUniqueNetIdRef& LocalUser, FString& OutJsonWebToken) = 0;

	// Function to async-verify a JWT, typically called on the server. ClaimedUser is the PUID the server believes this client to be (usually the NetConnection's FUniqueNetId); the SDK confirms the JWT is a valid signed token for that exact PUID. OnCompleted fires on the game thread: bSuccess=true means the token is authentic and belongs to ClaimedUser.
	DECLARE_DELEGATE_OneParam(FOnIdTokenVerified, bool /*bSuccess*/);
	virtual void VerifyIdToken(const FUniqueNetIdRef& ClaimedUser, const FString& JsonWebToken, FOnIdTokenVerified OnCompleted) = 0;
};
