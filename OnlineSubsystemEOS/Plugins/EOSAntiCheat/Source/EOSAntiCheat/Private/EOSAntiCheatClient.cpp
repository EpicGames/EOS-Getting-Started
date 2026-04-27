// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSAntiCheatClient.h"
#include "IEOSAntiCheat.h"

#include "OnlineSubsystemEOSTypesPublic.h"  // IUniqueNetIdEOS - read ProductUserId for LocalUser.
#include "EOSShared.h"                       // LexToString(EOS_ProductUserId).

#include "eos_anticheatclient.h"
#include "eos_common.h"

FEOSAntiCheatClient::FEOSAntiCheatClient(EOS_HPlatform InPlatform)
{
	Handle = EOS_Platform_GetAntiCheatClientInterface(InPlatform);
	if (!Handle)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient] EOS_Platform_GetAntiCheatClientInterface returned null - expected when the game wasn't launched via start_protected_game.exe."));
		return;
	}

	// Notification IDs are interface-handle scoped, not session scoped - bind
	// once at construction so the SDK has handlers wired the moment any
	// callback could fire. Handlers gate on bSessionActive where appropriate.
	{
		EOS_AntiCheatClient_AddNotifyMessageToServerOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOSERVER_API_LATEST;
		MessageToServerNotifyId = EOS_AntiCheatClient_AddNotifyMessageToServer(Handle, &Opts, this, &FEOSAntiCheatClient::OnMessageToServerStatic);
	}
	{
		EOS_AntiCheatClient_AddNotifyClientIntegrityViolatedOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYCLIENTINTEGRITYVIOLATED_API_LATEST;
		ClientIntegrityViolatedNotifyId = EOS_AntiCheatClient_AddNotifyClientIntegrityViolated(Handle, &Opts, this, &FEOSAntiCheatClient::OnClientIntegrityViolatedStatic);
	}
}

FEOSAntiCheatClient::~FEOSAntiCheatClient()
{
	if (bSessionActive)
	{
		EndSession();
	}

	if (Handle)
	{
		if (MessageToServerNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyMessageToServer(Handle, MessageToServerNotifyId);
			MessageToServerNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
		if (ClientIntegrityViolatedNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated(Handle, ClientIntegrityViolatedNotifyId);
			ClientIntegrityViolatedNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
	}
}

bool FEOSAntiCheatClient::BeginSession(EMode Mode, const FUniqueNetIdRef& LocalUser)
{
	if (!Handle)
	{
		return false;
	}
	if (bSessionActive)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::BeginSession] Session already active - expected on post-travel delegate rebind."));
		return false;
	}

	const IUniqueNetIdEOS& EosId = static_cast<const IUniqueNetIdEOS&>(*LocalUser);
	const EOS_ProductUserId LocalPuid = EosId.GetProductUserId();
	if (!EOS_ProductUserId_IsValid(LocalPuid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::BeginSession] Invalid local PUID."));
		return false;
	}

	EOS_AntiCheatClient_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
	Options.LocalUserId = LocalPuid;
	Options.Mode = (Mode == EMode::PeerToPeer) ? EOS_EAntiCheatClientMode::EOS_ACCM_PeerToPeer : EOS_EAntiCheatClientMode::EOS_ACCM_ClientServer;

	const EOS_EResult Result = EOS_AntiCheatClient_BeginSession(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::BeginSession] EOS_AntiCheatClient_BeginSession failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}

	bSessionActive = true;
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::BeginSession] Session started (mode=%s, localUser=%s)."),
		Mode == EMode::PeerToPeer ? TEXT("PeerToPeer") : TEXT("ClientServer"),
		*LexToString(LocalPuid));
	return true;
}

void FEOSAntiCheatClient::EndSession()
{
	if (!Handle || !bSessionActive)
	{
		return;
	}

	EOS_AntiCheatClient_EndSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_ENDSESSION_API_LATEST;
	EOS_AntiCheatClient_EndSession(Handle, &Options);

	bSessionActive = false;
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::EndSession] Session ended."));
}

void FEOSAntiCheatClient::ReceiveMessageFromServer(TConstArrayView<uint8> MessageBytes)
{
	if (!Handle || !bSessionActive || MessageBytes.Num() == 0)
	{
		return;
	}

	EOS_AntiCheatClient_ReceiveMessageFromServerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMSERVER_API_LATEST;
	Options.DataLengthBytes = static_cast<uint32_t>(MessageBytes.Num());
	Options.Data = MessageBytes.GetData();

	const EOS_EResult Result = EOS_AntiCheatClient_ReceiveMessageFromServer(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::ReceiveMessageFromServer] SDK rejected %d bytes: %s"), MessageBytes.Num(), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEOSAntiCheatPlugin, VeryVerbose, TEXT("[FEOSAntiCheatClient::ReceiveMessageFromServer] server -> client, %d bytes."), MessageBytes.Num());
#endif
}

/* static */ void EOS_CALL FEOSAntiCheatClient::OnMessageToServerStatic(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnMessageToServer(*Data);
	}
}

/* static */ void EOS_CALL FEOSAntiCheatClient::OnClientIntegrityViolatedStatic(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnClientIntegrityViolated(*Data);
	}
}

void FEOSAntiCheatClient::OnMessageToServer(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo& Info)
{
	TArray<uint8> Bytes;
	Bytes.Append(static_cast<const uint8*>(Info.MessageData), static_cast<int32>(Info.MessageDataSizeBytes));

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEOSAntiCheatPlugin, VeryVerbose, TEXT("[FEOSAntiCheatClient::OnMessageToServer] client -> server, %d bytes."), Bytes.Num());
#endif

	MessageToServerDelegate.Broadcast(Bytes);
}

void FEOSAntiCheatClient::OnClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo& Info)
{
	// This fires for local-process integrity failures (hook detection, debugger,
	// tamper). The SDK wants us to display the message and terminate. The
	// server is already informed independently; here we just log loudly. In a
	// shippable title we'd also invoke RequestExit after showing a modal.
	const FString ViolationMessage = Info.ViolationMessage ? ANSI_TO_TCHAR(Info.ViolationMessage) : TEXT("(none)");
	UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::OnClientIntegrityViolated] Local integrity violation (type=%d): %s"),
		static_cast<int32>(Info.ViolationType), *ViolationMessage);
}
