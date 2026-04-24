// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEOSAntiCheatClient.h"

// Private header so it's fine to pull the SDK umbrella - the public
// IEOSAntiCheatClient surface stays SDK-free.
#include "eos_sdk.h"

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

private:
	static void EOS_CALL OnMessageToServerStatic(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data);
	static void EOS_CALL OnClientIntegrityViolatedStatic(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data);

	void OnMessageToServer(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo& Info);
	void OnClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo& Info);

	EOS_HAntiCheatClient Handle = nullptr;
	bool bSessionActive = false;

	EOS_NotificationId MessageToServerNotifyId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId ClientIntegrityViolatedNotifyId = EOS_INVALID_NOTIFICATIONID;

	FOnMessageToServer MessageToServerDelegate;
};
