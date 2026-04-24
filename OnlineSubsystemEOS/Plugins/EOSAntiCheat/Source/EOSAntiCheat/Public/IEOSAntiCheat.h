// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "EOSAntiCheatDelegates.h"

class IEOSAntiCheatServer;
class IEOSAntiCheatClient;

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
 */
class EOSANTICHEAT_API IEOSAntiCheat : public IModuleInterface
{
public:
	/**
	 * Returns the singleton, or nullptr if the module isn't loaded. Safe to
	 * call from gameplay code; internally just a LoadModulePtr lookup.
	 */
	static IEOSAntiCheat* Get();

	/** Server-side wrapper, or null if this build / configuration can't host one. */
	virtual IEOSAntiCheatServer* GetServer() = 0;

	/** Client-side wrapper, or null if this build / configuration can't host one. */
	virtual IEOSAntiCheatClient* GetClient() = 0;

	/**
	 * Fires on the game thread when the server-side AntiCheat interface flags
	 * and kicks a player. See EOSAntiCheatDelegates.h for the payload.
	 */
	virtual FOnAntiCheatViolation& OnViolation() = 0;

	/**
	 * Copy a signed JWT for the local user. The client hands this to the server
	 * so the server can call VerifyIdToken before trusting the claimed PUID.
	 * Returns false if the local user isn't Connect-logged-in yet.
	 */
	virtual bool CopyLocalIdToken(const FUniqueNetIdRef& LocalUser, FString& OutJsonWebToken) = 0;

	/**
	 * Async JWT verify, typically called on the server. ClaimedUser is the PUID
	 * the server believes this client to be (usually the NetConnection's
	 * FUniqueNetId); the SDK confirms the JWT is a valid signed token for that
	 * exact PUID. OnCompleted fires on the game thread: bSuccess=true means the
	 * token is authentic and belongs to ClaimedUser.
	 */
	DECLARE_DELEGATE_OneParam(FOnIdTokenVerified, bool /*bSuccess*/);
	virtual void VerifyIdToken(const FUniqueNetIdRef& ClaimedUser, const FString& JsonWebToken, FOnIdTokenVerified OnCompleted) = 0;
};
