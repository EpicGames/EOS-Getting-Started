// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

#if !P2PMODE

/* =============== Tutorial 10 - Anti-Cheat (server interface) ============================= */

/**
 * Public surface of the plugin's server-side AntiCheat wrapper. Wraps an
 * EOS_HAntiCheatServer handle and exposes lifecycle, per-client registration,
 * and a bidirectional message-relay contract that the game is expected to
 * carry over its own network transport.
 *
 * The SDK fires all its callbacks on the game thread (from inside
 * EOS_Platform_Tick), so every delegate advertised on this interface is
 * game-thread safe to bind to and does not need additional synchronization.
 */
class IEOSAntiCheatServer
{
public:
	virtual ~IEOSAntiCheatServer() = default;

	// Function to start the AntiCheat server session. Must be called before RegisterClient and callback bindings produce useful output. Returns false if the SDK rejects the call (e.g. session already running).
	virtual bool BeginSession() = 0;

	// Function to tear down the AntiCheat server session and drop any registered clients.
	virtual void EndSession() = 0;

	// Function to register a player with the AntiCheat server. The player must be *ready to process messages* before this is called - calling RegisterClient while the client is still loading a map, blocked on I/O, or otherwise unable to heartbeat within the SDK's RegisterTimeoutSeconds window will trip an automatic timeout and the client will be rejected. The intended flow is: server starts session, client finishes loading, client pings the server with a "ready" RPC, *then* server calls RegisterClient. Returns false if the player is already registered or the call was rejected by the SDK.
	virtual bool RegisterClient(const FUniqueNetIdRef& PlayerId) = 0;

	// Function to remove a previously-registered player. No-op if not registered.
	virtual void UnregisterClient(const FUniqueNetIdRef& PlayerId) = 0;

	// Function to feed an opaque message received from a client (through the game's own network transport) back into the SDK for processing.
	virtual void ReceiveMessageFromClient(const FUniqueNetIdRef& PlayerId, TConstArrayView<uint8> MessageBytes) = 0;

	// Multicast accessor. Broadcasts when the SDK produces a message to deliver to a specific client. Subscribers are responsible for transporting Bytes to that player via the game's own reliable channel (e.g. a Client_ RPC).
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMessageToClient, const FUniqueNetIdRef& /*PlayerId*/, const TArray<uint8>& /*Bytes*/);
	virtual FOnMessageToClient& OnMessageToClient() = 0;
};

#endif // !P2PMODE
