// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEOSAntiCheatClient.h"

// Private header - safe to pull the SDK umbrella here.
#include "eos_sdk.h"

#if P2PMODE
#include "Containers/Ticker.h"
#include "eos_anticheatcommon_types.h"
#include "eos_p2p_types.h"
#endif

class FEOSAntiCheatClient final : public IEOSAntiCheatClient
{
public:
	explicit FEOSAntiCheatClient(EOS_HPlatform InPlatform);
	virtual ~FEOSAntiCheatClient() override;

	// IEOSAntiCheatClient
	virtual bool BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser) override;
	virtual void EndSession() override;
	virtual void ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes) override;
	virtual FOnMessageToServer& OnMessageToServer() override { return MessageToServerDelegate; }

#if P2PMODE
	virtual bool RegisterPeer(const FUniqueNetIdRef& PeerUser) override;
	virtual void UnregisterPeer(const FUniqueNetIdRef& PeerUser) override;
	virtual FOnPeerActionRequired& OnPeerActionRequired() override { return PeerActionRequiredDelegate; }
#endif

private:
	static void EOS_CALL OnMessageToServerStatic(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data);
	static void EOS_CALL OnClientIntegrityViolatedStatic(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data);

	void OnMessageToServer(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo& Info);
	void OnClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo& Info);

	EOS_HPlatform PlatformHandle = nullptr;
	EOS_HAntiCheatClient Handle = nullptr;
	bool bSessionActive = false;

	EOS_NotificationId MessageToServerNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId ClientIntegrityViolatedNotifyId = EOS_INVALID_NOTIFICATIONID;

	FOnMessageToServer MessageToServerDelegate;

#if P2PMODE
	// P2P listen-server state. Raw EOS_P2P_* (FSocketEOS's SetSocketName /
	// SetLocalAddress aren't exported in 5.8). SDK reuses shared callback types.
	static void EOS_CALL OnMessageToPeerStatic(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data);
	static void EOS_CALL OnPeerActionRequiredStatic(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data);
	static void EOS_CALL OnPeerAuthStatusChangedStatic(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data);
	static void EOS_CALL OnP2PConnectionRequestStatic(const EOS_P2P_OnIncomingConnectionRequestInfo* Data);

	void OnMessageToPeer(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo& Info);
	void OnPeerActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo& Info);
	void OnPeerAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo& Info);
	void OnP2PConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo& Info);

	// Drains EOS_P2P_ReceivePacket on the AC channel into ReceiveMessageFromPeer.
	bool TickReceivePump(float DeltaSeconds);

	void BeginP2PSession(const FUniqueNetIdRef& LocalUser);
	void EndP2PSession();

	FOnPeerActionRequired PeerActionRequiredDelegate;

	EOS_HP2P P2PHandle = nullptr;
	EOS_ProductUserId LocalPuid = nullptr;
	EOS_P2P_SocketId AntiCheatSocketId = {};  // initialized in ctor; constant for the wrapper's lifetime
	uint8 AntiCheatChannelHash = 0;            // ditto - hash of the socket name
	TMap<EOS_ProductUserId, FUniqueNetIdRef> PeerByProductId;

	EOS_NotificationId MessageToPeerNotifyId         = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerActionRequiredNotifyId    = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerAuthStatusChangedNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId P2PConnectionRequestNotifyId  = EOS_INVALID_NOTIFICATIONID;

	FTSTicker::FDelegateHandle ReceivePumpHandle;
#endif // P2PMODE
};
