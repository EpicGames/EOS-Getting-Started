// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "IEOSAntiCheatClient.h"

// Private header so it's fine to pull the SDK umbrella - the public surface stays SDK-free.
#include "eos_sdk.h"

#if P2PMODE
#include "Containers/Ticker.h"
#include "eos_anticheatcommon_types.h"
#include "eos_p2p_types.h"
#endif

/* =============== Tutorial 10 - Anti-Cheat (client impl) ============================= */

class FEOSAntiCheatClient final : public IEOSAntiCheatClient
{
public:
	explicit FEOSAntiCheatClient(EOS_HPlatform InPlatform);
	virtual ~FEOSAntiCheatClient() override;

	// IEOSAntiCheatClient
	virtual bool BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser) override;
	virtual void EndSession() override;

#if !P2PMODE
	virtual void ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes) override;
	virtual FOnMessageToServer& OnMessageToServer() override { return MessageToServerDelegate; }
#endif

#if P2PMODE
	virtual bool RegisterPeer(const FUniqueNetIdRef& PeerUser) override;
	virtual void UnregisterPeer(const FUniqueNetIdRef& PeerUser) override;
	virtual FOnPeerActionRequired& OnPeerActionRequired() override { return PeerActionRequiredDelegate; }
#endif

private:
	// SDK -> C++ trampolines. Each forwards the call to the instance pointed to by ClientData (which we set to `this` when binding).
	static void EOS_CALL OnClientIntegrityViolatedStatic(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data);

	// Callback function. Ran when the SDK detects a local-client integrity violation.
	void OnClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo& Info);

	EOS_HPlatform PlatformHandle = nullptr;
	EOS_HAntiCheatClient Handle = nullptr;
	bool bSessionActive = false;

	// Notification IDs we need to release in EndSession / dtor.
	EOS_NotificationId ClientIntegrityViolatedNotifyId = EOS_INVALID_NOTIFICATIONID;

#if !P2PMODE
	// Client-server transport (P2PMODE=0): SDK -> C++ trampoline + handler + delegate, all #if !P2PMODE because the SDK only fires MessageToServer in ClientServer mode.
	static void EOS_CALL OnMessageToServerStatic(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data);
	void OnMessageToServer(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo& Info);
	EOS_NotificationId MessageToServerNotifyId = EOS_INVALID_NOTIFICATIONID;
	FOnMessageToServer MessageToServerDelegate;
#endif

#if P2PMODE
	/* =============== Tutorial 10 - Anti-Cheat (P2P listen-server state, P2PMODE=1 only) ============================= */
	// Raw EOS_P2P_* (FSocketEOS's SetSocketName / SetLocalAddress aren't exported in 5.8). SDK reuses shared callback types.

	// SDK -> C++ trampolines (P2P branch).
	static void EOS_CALL OnMessageToPeerStatic(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data);
	static void EOS_CALL OnPeerActionRequiredStatic(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data);
	static void EOS_CALL OnPeerAuthStatusChangedStatic(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data);
	static void EOS_CALL OnP2PConnectionRequestStatic(const EOS_P2P_OnIncomingConnectionRequestInfo* Data);

	// Callback function. Ran when the SDK has bytes to send to a peer.
	void OnMessageToPeer(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo& Info);

	// Callback function. Ran when the SDK requests an action against a peer (e.g. RemovePlayer).
	void OnPeerActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo& Info);

	// Callback function. Ran when a peer's auth status changes.
	void OnPeerAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo& Info);

	// Callback function. Ran on incoming P2P connection requests for the AntiCheat socket; auto-accepts known peers.
	void OnP2PConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo& Info);

	// Function to drain EOS_P2P_ReceivePacket on the AC channel into ReceiveMessageFromPeer.
	bool TickReceivePump(float DeltaSeconds);

	// Function to begin the P2P-specific session state (socket setup, notification registration).
	void BeginP2PSession(const FUniqueNetIdRef& LocalUser);

	// Function to end the P2P-specific session state.
	void EndP2PSession();

	// Multicast for SDK-requested peer actions. Forwarded to subscribers (the game's lobby-kick path).
	FOnPeerActionRequired PeerActionRequiredDelegate;

	EOS_HP2P P2PHandle = nullptr;
	EOS_ProductUserId LocalPuid = nullptr;
	EOS_P2P_SocketId AntiCheatSocketId = {};  // initialized in ctor; constant for the wrapper's lifetime
	uint8 AntiCheatChannelHash = 0;            // ditto - hash of the socket name
	TMap<EOS_ProductUserId, FUniqueNetIdRef> PeerByProductId;

	// Notification IDs we need to release in EndP2PSession / dtor.
	EOS_NotificationId MessageToPeerNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerActionRequiredNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerAuthStatusChangedNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId P2PConnectionRequestNotifyId = EOS_INVALID_NOTIFICATIONID;

	// Ticker handle for the P2P receive pump.
	FTSTicker::FDelegateHandle ReceivePumpHandle;
#endif // P2PMODE
};
