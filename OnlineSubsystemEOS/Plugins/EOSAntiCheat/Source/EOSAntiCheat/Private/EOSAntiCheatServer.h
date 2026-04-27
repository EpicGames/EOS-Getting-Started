// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEOSAntiCheatServer.h"
#include "IEOSAntiCheat.h"  // FOnAntiCheatViolation

#if !P2PMODE

// Private header so it's fine to pull the SDK umbrella - the public
// IEOSAntiCheatServer surface stays SDK-free.
#include "eos_sdk.h"

class FEOSAntiCheatServer final : public IEOSAntiCheatServer
{
public:
	FEOSAntiCheatServer(EOS_HPlatform InPlatform, FOnAntiCheatViolation& InViolationDelegate);
	virtual ~FEOSAntiCheatServer() override;

	// IEOSAntiCheatServer
	virtual bool BeginSession() override;
	virtual void EndSession() override;
	virtual bool RegisterClient(const FUniqueNetIdRef& PlayerId) override;
	virtual void UnregisterClient(const FUniqueNetIdRef& PlayerId) override;
	virtual void ReceiveMessageFromClient(const FUniqueNetIdRef& PlayerId, TConstArrayView<uint8> MessageBytes) override;
	virtual FOnMessageToClient& OnMessageToClient() override { return MessageToClientDelegate; }

private:
	// SDK -> C++ trampolines. Each forwards the call to the instance pointed
	// to by ClientData (which we set to `this` when binding).
	static void EOS_CALL OnMessageToClientStatic(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data);
	static void EOS_CALL OnClientActionRequiredStatic(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data);
	static void EOS_CALL OnClientAuthStatusChangedStatic(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data);

	// Instance handlers invoked from the static trampolines above.
	void OnMessageToClient(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo& Info);
	void OnClientActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo& Info);
	void OnClientAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo& Info);

	/** Funnels both violation callbacks into one kick path + OnViolation broadcast. */
	void FlagForKick(EOS_AntiCheatCommon_ClientHandle ClientHandle, const FString& Reason);

	EOS_HAntiCheatServer Handle = nullptr;
	bool bSessionActive = false;

	// Maps PUID -> owning NetIdRef. We use the PUID pointer itself as the
	// ClientHandle value we hand to the SDK (it's already a stable opaque
	// pointer for the session's lifetime), so the reverse lookup from a
	// callback's ClientHandle is just this map indexed by PUID.
	TMap<EOS_ProductUserId, FUniqueNetIdRef> Registered;

	// Notification IDs we need to release in EndSession / dtor.
	EOS_NotificationId MessageToClientNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId ClientActionRequiredNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId ClientAuthStatusChangedNotifyId = EOS_INVALID_NOTIFICATIONID;

	FOnMessageToClient MessageToClientDelegate;

	// Back-reference to the module-level violation delegate so we can broadcast
	// from inside our callbacks without a runtime IEOSAntiCheat::Get() lookup.
	FOnAntiCheatViolation& ViolationDelegate;
};

#endif // !P2PMODE
