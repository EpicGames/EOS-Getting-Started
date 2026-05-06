// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

/* =============== Tutorial 10 - Anti-Cheat (client interface) ============================= */

/**
 * Client-side AntiCheat wrapper around EOS_HAntiCheatClient. Two modes:
 *   - ClientServer: client talks to a trusted dedicated server. Game owns
 *                   message transport (ReceiveMessageFromServer / OnMessageToServer).
 *   - PeerToPeer:   no dedicated EAC Server; every peer runs EAC Client and exchanges
 *                   handshake messages with every other peer ("full-mesh" at the EAC
 *                   peer-auth layer, NOT at UE replication). Plugin owns transport on a
 *                   dedicated EOS P2P socket.
 * SDK callbacks fire on the game thread via EOS_Platform_Tick.
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

	// Function to start the session. Call after EOS Connect login (LocalUser is the resulting PUID).
	virtual bool BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser) = 0;

	// Function to tear down the client AntiCheat session.
	virtual void EndSession() = 0;

#if !P2PMODE
	/* =============== Tutorial 10 - Anti-Cheat (client/server transport, P2PMODE=0 only) ============================= */

	// Function to feed bytes received from the server (via game RPC) into the SDK.
	virtual void ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes) = 0;

	// Multicast accessor. Fires when the SDK has bytes to send to the server. Subscriber owns transport (e.g. reliable RPC).
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageToServer, const TArray<uint8>& /*Bytes*/);
	virtual FOnMessageToServer& OnMessageToServer() = 0;
#endif // !P2PMODE

#if P2PMODE
	/* =============== Tutorial 10 - Anti-Cheat (P2P peer surface, P2PMODE=1 only) ============================= */

	// Function to register a peer for the EAC peer-auth channel. PeerUser is the lobby-participant PUID; EAC runs its own handshake on top.
	virtual bool RegisterPeer(const FUniqueNetIdRef& PeerUser) = 0;

	// Function to unregister a peer from the EAC peer-auth channel.
	virtual void UnregisterPeer(const FUniqueNetIdRef& PeerUser) = 0;

	// Multicast accessor. Fires when the SDK requests a RemovePlayer action. Peer is the offending PUID, or null when the LOCAL client was flagged (PEER_SELF).
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPeerActionRequired, const FUniqueNetIdPtr& /*Peer, null = self*/, const FString& /*Reason*/);
	virtual FOnPeerActionRequired& OnPeerActionRequired() = 0;
#endif // P2PMODE
};
