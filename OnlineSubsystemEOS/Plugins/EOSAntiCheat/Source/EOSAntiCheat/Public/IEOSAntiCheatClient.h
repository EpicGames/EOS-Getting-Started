// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

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

	/** Start the session. Call after EOS Connect login (LocalUser is the resulting PUID). */
	virtual bool BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser) = 0;

	/** Tear down the client AntiCheat session. */
	virtual void EndSession() = 0;

	/** Feed bytes received from the server (via game RPC) into the SDK. */
	virtual void ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes) = 0;

	/** Fires when the SDK has bytes to send to the server. Subscriber owns transport (e.g. reliable RPC). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageToServer, const TArray<uint8>& /*Bytes*/);
	virtual FOnMessageToServer& OnMessageToServer() = 0;

#if P2PMODE
	/** Register/unregister peers for the EAC peer-auth channel. PeerUser is the lobby-participant PUID; EAC runs its own handshake on top. */
	virtual bool RegisterPeer(const FUniqueNetIdRef& PeerUser) = 0;
	virtual void UnregisterPeer(const FUniqueNetIdRef& PeerUser) = 0;

	/** RemovePlayer action from the SDK. Peer is the offending PUID, or null when the LOCAL client was flagged (PEER_SELF). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPeerActionRequired, const FUniqueNetIdPtr& /*Peer, null = self*/, const FString& /*Reason*/);
	virtual FOnPeerActionRequired& OnPeerActionRequired() = 0;
#endif // P2PMODE
};
