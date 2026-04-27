// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSAntiCheatServer.h"
#include "IEOSAntiCheat.h"

#include "OnlineSubsystemEOSTypesPublic.h"  // IUniqueNetIdEOS - to read ProductUserId off game-side FUniqueNetIds.
#include "EOSShared.h"                       // LexToString(EOS_ProductUserId).

#include "eos_anticheatserver.h"
#include "eos_common.h"

namespace
{
	/** Short human-readable text for the ClientAction enum (for logs + kick reason). */
	// The SDK exposes these enums as `enum class` under C++, so every case label
	// has to be fully qualified with the enum type - hence the verbose names.
	FString LexToStringClientAction(EOS_EAntiCheatCommonClientAction Action)
	{
		switch (Action)
		{
		case EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer: return TEXT("RemovePlayer");
		default:                                                       return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Action));
		}
	}

	FString LexToStringAuthStatus(EOS_EAntiCheatCommonClientAuthStatus Status)
	{
		switch (Status)
		{
		case EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_Invalid:            return TEXT("Invalid");
		case EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_LocalAuthComplete:  return TEXT("LocalAuthComplete");
		case EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_RemoteAuthComplete: return TEXT("RemoteAuthComplete");
		default:                                                                  return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Status));
		}
	}
}

FEOSAntiCheatServer::FEOSAntiCheatServer(EOS_HPlatform InPlatform, FOnAntiCheatViolation& InViolationDelegate)
	: ViolationDelegate(InViolationDelegate)
{
	Handle = EOS_Platform_GetAntiCheatServerInterface(InPlatform);
	if (!Handle)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatServer] EOS_Platform_GetAntiCheatServerInterface returned null."));
		return;
	}

	// Notification IDs are interface-handle scoped, not session scoped - bind
	// once at construction so the SDK has handlers wired the moment any
	// callback could fire. Handlers gate on bSessionActive where appropriate.
	{
		EOS_AntiCheatServer_AddNotifyMessageToClientOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYMESSAGETOCLIENT_API_LATEST;
		MessageToClientNotifyId = EOS_AntiCheatServer_AddNotifyMessageToClient(Handle, &Opts, this, &FEOSAntiCheatServer::OnMessageToClientStatic);
	}
	{
		EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTACTIONREQUIRED_API_LATEST;
		ClientActionRequiredNotifyId = EOS_AntiCheatServer_AddNotifyClientActionRequired(Handle, &Opts, this, &FEOSAntiCheatServer::OnClientActionRequiredStatic);
	}
	{
		EOS_AntiCheatServer_AddNotifyClientAuthStatusChangedOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTAUTHSTATUSCHANGED_API_LATEST;
		ClientAuthStatusChangedNotifyId = EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged(Handle, &Opts, this, &FEOSAntiCheatServer::OnClientAuthStatusChangedStatic);
	}
}

FEOSAntiCheatServer::~FEOSAntiCheatServer()
{
	if (bSessionActive)
	{
		EndSession();
	}

	if (Handle)
	{
		if (MessageToClientNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyMessageToClient(Handle, MessageToClientNotifyId);
			MessageToClientNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
		if (ClientActionRequiredNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyClientActionRequired(Handle, ClientActionRequiredNotifyId);
			ClientActionRequiredNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
		if (ClientAuthStatusChangedNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged(Handle, ClientAuthStatusChangedNotifyId);
			ClientAuthStatusChangedNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
	}
}

bool FEOSAntiCheatServer::BeginSession()
{
	if (!Handle)
	{
		return false;
	}
	if (bSessionActive)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::BeginSession] Session already active - skipping duplicate start."));
		return false;
	}

	EOS_AntiCheatServer_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATSERVER_BEGINSESSION_API_LATEST;
	Options.RegisterTimeoutSeconds = 60; // recommended default
	Options.ServerName = nullptr;
	Options.bEnableGameplayData = EOS_FALSE; // we don't use LogPlayerTick in this tutorial
	Options.LocalUserId = nullptr;           // dedicated server has no local user

	const EOS_EResult Result = EOS_AntiCheatServer_BeginSession(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatServer::BeginSession] EOS_AntiCheatServer_BeginSession failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}

	bSessionActive = true;
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::BeginSession] Session started."));
	return true;
}

void FEOSAntiCheatServer::EndSession()
{
	if (!Handle || !bSessionActive)
	{
		return;
	}

	EOS_AntiCheatServer_EndSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATSERVER_ENDSESSION_API_LATEST;
	EOS_AntiCheatServer_EndSession(Handle, &Options);

	Registered.Empty();
	bSessionActive = false;

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::EndSession] Session ended."));
}

bool FEOSAntiCheatServer::RegisterClient(const FUniqueNetIdRef& PlayerId)
{
	if (!Handle || !bSessionActive)
	{
		return false;
	}

	const IUniqueNetIdEOS& EosId = static_cast<const IUniqueNetIdEOS&>(*PlayerId);
	const EOS_ProductUserId Puid = EosId.GetProductUserId();
	if (!EOS_ProductUserId_IsValid(Puid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatServer::RegisterClient] Invalid PUID on PlayerId=%s."), *PlayerId->ToString());
		return false;
	}

	if (Registered.Contains(Puid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatServer::RegisterClient] PUID %s already registered."), *LexToString(Puid));
		return false;
	}

	EOS_AntiCheatServer_RegisterClientOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATSERVER_REGISTERCLIENT_API_LATEST;
	// The SDK treats ClientHandle as an opaque void* identifying the client.
	// Reusing the stable EOS_ProductUserId pointer saves us a parallel handle
	// pool - the callbacks hand it back and we look the NetId up from Registered.
	Options.ClientHandle = static_cast<EOS_AntiCheatCommon_ClientHandle>(Puid);
	Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
	Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Unknown;
	Options.AccountId_DEPRECATED = nullptr;
	Options.IpAddress = nullptr; // optional
	Options.UserId = Puid;
	Options.Reserved01 = 0;

	const EOS_EResult Result = EOS_AntiCheatServer_RegisterClient(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatServer::RegisterClient] RegisterClient failed for %s: %s"), *LexToString(Puid), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}

	Registered.Add(Puid, PlayerId);
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::RegisterClient] Registered %s."), *LexToString(Puid));
	return true;
}

void FEOSAntiCheatServer::UnregisterClient(const FUniqueNetIdRef& PlayerId)
{
	if (!Handle || !bSessionActive)
	{
		return;
	}

	const IUniqueNetIdEOS& EosId = static_cast<const IUniqueNetIdEOS&>(*PlayerId);
	const EOS_ProductUserId Puid = EosId.GetProductUserId();
	if (!Registered.Contains(Puid))
	{
		return;
	}

	EOS_AntiCheatServer_UnregisterClientOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATSERVER_UNREGISTERCLIENT_API_LATEST;
	Options.ClientHandle = static_cast<EOS_AntiCheatCommon_ClientHandle>(Puid);
	EOS_AntiCheatServer_UnregisterClient(Handle, &Options);

	Registered.Remove(Puid);
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::UnregisterClient] Unregistered %s."), *LexToString(Puid));
}

void FEOSAntiCheatServer::ReceiveMessageFromClient(const FUniqueNetIdRef& PlayerId, TConstArrayView<uint8> MessageBytes)
{
	if (!Handle || !bSessionActive || MessageBytes.Num() == 0)
	{
		return;
	}

	const IUniqueNetIdEOS& EosId = static_cast<const IUniqueNetIdEOS&>(*PlayerId);
	const EOS_ProductUserId Puid = EosId.GetProductUserId();

	EOS_AntiCheatServer_ReceiveMessageFromClientOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATSERVER_RECEIVEMESSAGEFROMCLIENT_API_LATEST;
	Options.ClientHandle = static_cast<EOS_AntiCheatCommon_ClientHandle>(Puid);
	Options.DataLengthBytes = static_cast<uint32_t>(MessageBytes.Num());
	Options.Data = MessageBytes.GetData();

	const EOS_EResult Result = EOS_AntiCheatServer_ReceiveMessageFromClient(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatServer::ReceiveMessageFromClient] %s rejected (%s, %d bytes)."),
			*LexToString(Puid), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)), MessageBytes.Num());
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEOSAntiCheatPlugin, VeryVerbose, TEXT("[FEOSAntiCheatServer::ReceiveMessageFromClient] %s -> server, %d bytes."), *LexToString(Puid), MessageBytes.Num());
#endif
}

/* static */ void EOS_CALL FEOSAntiCheatServer::OnMessageToClientStatic(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatServer*>(Data->ClientData)->OnMessageToClient(*Data);
	}
}

/* static */ void EOS_CALL FEOSAntiCheatServer::OnClientActionRequiredStatic(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatServer*>(Data->ClientData)->OnClientActionRequired(*Data);
	}
}

/* static */ void EOS_CALL FEOSAntiCheatServer::OnClientAuthStatusChangedStatic(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatServer*>(Data->ClientData)->OnClientAuthStatusChanged(*Data);
	}
}

void FEOSAntiCheatServer::OnMessageToClient(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo& Info)
{
	const EOS_ProductUserId Puid = static_cast<EOS_ProductUserId>(Info.ClientHandle);
	const FUniqueNetIdRef* PlayerIdPtr = Registered.Find(Puid);
	if (!PlayerIdPtr)
	{
		// SDK produced a message for a client we no longer track; nothing to do.
		return;
	}

	TArray<uint8> Bytes;
	Bytes.Append(static_cast<const uint8*>(Info.MessageData), static_cast<int32>(Info.MessageDataSizeBytes));

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEOSAntiCheatPlugin, VeryVerbose, TEXT("[FEOSAntiCheatServer::OnMessageToClient] server -> %s, %d bytes."), *LexToString(Puid), Bytes.Num());
#endif

	MessageToClientDelegate.Broadcast(*PlayerIdPtr, Bytes);
}

void FEOSAntiCheatServer::OnClientActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo& Info)
{
	if (Info.ClientAction != EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
	{
		// Current SDK only ever asks us to remove players; log anything else as a
		// future-proofing signal without kicking.
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatServer::OnClientActionRequired] Unexpected action %s ignored."), *LexToStringClientAction(Info.ClientAction));
		return;
	}

	const FString Reason = FString::Printf(TEXT("AntiCheat: %s (%s)"),
		*LexToStringClientAction(Info.ClientAction),
		Info.ActionReasonDetailsString ? ANSI_TO_TCHAR(Info.ActionReasonDetailsString) : TEXT("no detail"));

	FlagForKick(Info.ClientHandle, Reason);
}

void FEOSAntiCheatServer::OnClientAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo& Info)
{
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatServer::OnClientAuthStatusChanged] %s auth status -> %s."),
		*LexToString(static_cast<EOS_ProductUserId>(Info.ClientHandle)),
		*LexToStringAuthStatus(Info.ClientAuthStatus));

	if (Info.ClientAuthStatus == EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_Invalid)
	{
		const FString Reason = FString::Printf(TEXT("AntiCheat: auth status %s"), *LexToStringAuthStatus(Info.ClientAuthStatus));
		FlagForKick(Info.ClientHandle, Reason);
	}
}

void FEOSAntiCheatServer::FlagForKick(EOS_AntiCheatCommon_ClientHandle ClientHandle, const FString& Reason)
{
	const EOS_ProductUserId Puid = static_cast<EOS_ProductUserId>(ClientHandle);
	const FUniqueNetIdRef* PlayerIdPtr = Registered.Find(Puid);
	if (!PlayerIdPtr)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatServer::FlagForKick] Kick requested for unknown PUID %s - ignoring."), *LexToString(Puid));
		return;
	}

	UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatServer::FlagForKick] Player %s flagged: %s"), *(*PlayerIdPtr)->ToString(), *Reason);

	// The plugin only raises the event - the game decides what to do (kick,
	// ban, log telemetry). AEOSGameSession subscribes and calls KickPlayer.
	ViolationDelegate.Broadcast(*PlayerIdPtr, Reason);
}
