// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

/**
 * Public surface of the plugin's client-side AntiCheat wrapper. Wraps an
 * EOS_HAntiCheatClient handle. The game owns the message transport and
 * relays opaque byte blobs in both directions.
 *
 * The client-side session runs in one of two modes set at BeginSession time:
 *   - ClientServer: the client talks to a trusted dedicated / listen server.
 *   - PeerToPeer:   the client is part of a full-mesh peer group. Requires the
 *                   P2P-only APIs (RegisterPeer, OnMessageToPeer, etc.) which
 *                   will be added alongside the P2P mode integration.
 *
 * All SDK callbacks fire on the game thread via EOS_Platform_Tick.
 */
class IEOSAntiCheatClient
{
public:
	virtual ~IEOSAntiCheatClient() = default;

	enum class EMode : uint8
	{
		ClientServer,
		PeerToPeer,
	};

	/**
	 * Start the client AntiCheat session. Must be called after the local
	 * EOS Connect login has completed (LocalUser is the resulting PUID).
	 * Mode decides which SDK topology the session operates under; see the
	 * class comment.
	 */
	virtual bool BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser) = 0;

	/** Tear down the client AntiCheat session. */
	virtual void EndSession() = 0;

	/**
	 * Feed an opaque message received from the server (through the game's
	 * own network transport) into the SDK for processing.
	 */
	virtual void ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes) = 0;

	/**
	 * Broadcast when the SDK produces a message to send to the server.
	 * Subscribers are responsible for transporting Bytes over the game's
	 * reliable channel (e.g. a Server_ RPC).
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageToServer, const TArray<uint8>& /*Bytes*/);
	virtual FOnMessageToServer& OnMessageToServer() = 0;
};
