// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSAntiCheatClient.h"
#include "IEOSAntiCheat.h"

#include "OnlineSubsystemEOSTypesPublic.h"  // IUniqueNetIdEOS - read ProductUserId for LocalUser.
#include "EOSShared.h"                       // LexToString(EOS_ProductUserId).

#include "eos_anticheatclient.h"
#include "eos_common.h"

#if P2PMODE
#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "Containers/Ticker.h"

// EAC PeerToPeer mode - every peer exchanges AC handshake messages with every other
// peer over a dedicated "EOSAntiCheat" EOS P2P socket, kept off NetDriverEOS's
// "GameNetDriver" socket. ("Full-mesh" describes EAC's peer-auth channel here, NOT
// UE replication - the UE topology is listen-server-over-P2P.) Raw EOS_P2P_* because
// FSocketEOS's SetSocketName / SetLocalAddress aren't exported in 5.8.
// ReceivePacket MUST filter on our channel hash or it steals NetDriverEOS
// packets and breaks travel. Channel hash matches FSocketEOS's channel-
// assignment rule, so AC traffic stays isolated from NetDriverEOS.
static constexpr const ANSICHAR* AntiCheatSocketName = "EOSAntiCheat";
#endif

FEOSAntiCheatClient::FEOSAntiCheatClient(EOS_HPlatform InPlatform)
{
	PlatformHandle = InPlatform;
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

#if P2PMODE
	// Cache the socket id + channel hash once - both are constant for the
	// wrapper's lifetime and get used on the per-send hot path.
	AntiCheatSocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(AntiCheatSocketId.SocketName, AntiCheatSocketName, EOS_P2P_SOCKETID_SOCKETNAME_SIZE);
	AntiCheatChannelHash = static_cast<uint8>(GetTypeHash(FString(ANSI_TO_TCHAR(AntiCheatSocketName))));

	// Peer notifies live on the same AC client handle - bind here too so the
	// SDK has handlers ready before the first PeerToPeer BeginSession.
	{
		EOS_AntiCheatClient_AddNotifyMessageToPeerOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOPEER_API_LATEST;
		MessageToPeerNotifyId = EOS_AntiCheatClient_AddNotifyMessageToPeer(Handle, &Opts, this, &FEOSAntiCheatClient::OnMessageToPeerStatic);
	}
	{
		EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERACTIONREQUIRED_API_LATEST;
		PeerActionRequiredNotifyId = EOS_AntiCheatClient_AddNotifyPeerActionRequired(Handle, &Opts, this, &FEOSAntiCheatClient::OnPeerActionRequiredStatic);
	}
	{
		EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions Opts = {};
		Opts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERAUTHSTATUSCHANGED_API_LATEST;
		PeerAuthStatusChangedNotifyId = EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(Handle, &Opts, this, &FEOSAntiCheatClient::OnPeerAuthStatusChangedStatic);
	}
#endif
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
#if P2PMODE
		if (MessageToPeerNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyMessageToPeer(Handle, MessageToPeerNotifyId);
			MessageToPeerNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
		if (PeerActionRequiredNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(Handle, PeerActionRequiredNotifyId);
			PeerActionRequiredNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
		if (PeerAuthStatusChangedNotifyId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(Handle, PeerAuthStatusChangedNotifyId);
			PeerAuthStatusChangedNotifyId = EOS_INVALID_NOTIFICATIONID;
		}
#endif
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
	const EOS_ProductUserId LocalUserPuid = EosId.GetProductUserId();
	if (!EOS_ProductUserId_IsValid(LocalUserPuid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::BeginSession] Invalid local PUID."));
		return false;
	}

	EOS_AntiCheatClient_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
	Options.LocalUserId = LocalUserPuid;
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
		*LexToString(LocalUserPuid));

#if P2PMODE
	if (Mode == EMode::PeerToPeer)
	{
		BeginP2PSession(LocalUser);
	}
#endif

	return true;
}

void FEOSAntiCheatClient::EndSession()
{
	if (!Handle || !bSessionActive)
	{
		return;
	}

#if P2PMODE
	EndP2PSession();
#endif

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

void EOS_CALL FEOSAntiCheatClient::OnMessageToServerStatic(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnMessageToServer(*Data);
	}
}

void EOS_CALL FEOSAntiCheatClient::OnClientIntegrityViolatedStatic(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data)
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
	// Local-process integrity failure (tamper, debugger, hook). Tutorial just logs.
	const FString ViolationMessage = Info.ViolationMessage ? ANSI_TO_TCHAR(Info.ViolationMessage) : TEXT("(none)");
	UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::OnClientIntegrityViolated] Local integrity violation (type=%d): %s"),
		static_cast<int32>(Info.ViolationType), *ViolationMessage);
}

#if P2PMODE

static FString DescribeActionReason(EOS_EAntiCheatCommonClientActionReason Code, const char* Details)
{
	const FString DetailStr = Details ? ANSI_TO_TCHAR(Details) : TEXT("(no details)");
	return FString::Printf(TEXT("%s (%d)"), *DetailStr, static_cast<int32>(Code));
}

void FEOSAntiCheatClient::BeginP2PSession(const FUniqueNetIdRef& LocalUser)
{
	// Local multi-client dev gotcha: when two EAC-protected processes start
	// up on the same host within ~1s of each other, both can land in a state
	// where peer-auth bytes never deliver between them - sockets connect
	// (EOS reports "Connection established") but the EAC client SDK on each
	// side never sees the other's bytes, both time out at AuthenticationTimeout.
	// Empirically: ~5s+ delay between protected launches makes it reliable.
	// EAC's host-shared service+driver appears to need that window to finish
	// attaching the first process before the second registers. Not a real-
	// deployment concern (players don't multi-instance protected clients on
	// one machine), but matters for local two-client testing.
	P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::BeginP2PSession] EOS_Platform_GetP2PInterface returned null - P2P AC socket disabled."));
		return;
	}
	LocalPuid = static_cast<const IUniqueNetIdEOS&>(*LocalUser).GetProductUserId();

	// Accept incoming AC-socket connections only from RegisterPeer'd peers.
	{
		EOS_P2P_AddNotifyPeerConnectionRequestOptions Opts = {};
		Opts.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
		Opts.LocalUserId = LocalPuid;
		Opts.SocketId = &AntiCheatSocketId;
		P2PConnectionRequestNotifyId = EOS_P2P_AddNotifyPeerConnectionRequest(P2PHandle, &Opts, this, &FEOSAntiCheatClient::OnP2PConnectionRequestStatic);
	}

	// ~30Hz pump - SDK tick alone won't drain the inbound packet queue.
	ReceivePumpHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FEOSAntiCheatClient::TickReceivePump),
		1.0f / 30.0f);

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::BeginP2PSession] P2P AC socket '%s' opened for localUser=%s."),
		ANSI_TO_TCHAR(AntiCheatSocketName), *LexToString(LocalPuid));
}

void FEOSAntiCheatClient::EndP2PSession()
{
	if (!P2PHandle)
	{
		return;
	}

	if (ReceivePumpHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReceivePumpHandle);
		ReceivePumpHandle.Reset();
	}

	// EAC peer notifies stay bound for the wrapper's lifetime (see ctor).
	// Only the P2P-socket notify is session-scoped (it filters on LocalPuid).
	if (P2PConnectionRequestNotifyId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_P2P_RemoveNotifyPeerConnectionRequest(P2PHandle, P2PConnectionRequestNotifyId);
		P2PConnectionRequestNotifyId = EOS_INVALID_NOTIFICATIONID;
	}

	PeerByProductId.Reset();
	P2PHandle = nullptr;
	LocalPuid = nullptr;

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::EndP2PSession] P2P AC socket closed."));
}

bool FEOSAntiCheatClient::RegisterPeer(const FUniqueNetIdRef& PeerUser)
{
	if (!Handle || !bSessionActive)
	{
		return false;
	}

	const IUniqueNetIdEOS& EosId = static_cast<const IUniqueNetIdEOS&>(*PeerUser);
	const EOS_ProductUserId PeerPuid = EosId.GetProductUserId();
	if (!EOS_ProductUserId_IsValid(PeerPuid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::RegisterPeer] Invalid peer PUID - skipping."));
		return false;
	}
	if (PeerByProductId.Contains(PeerPuid))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::RegisterPeer] Peer %s already registered - ignoring."), *LexToString(PeerPuid));
		return true;
	}

	// AuthenticationTimeout from config, clamped to SDK range [40s, 120s].
	int32 TimeoutFromIni = EOS_ANTICHEATCLIENT_REGISTERPEER_MIN_AUTHENTICATIONTIMEOUT;
	GConfig->GetInt(TEXT("EOSAntiCheat"), TEXT("PeerAuthenticationTimeoutSeconds"), TimeoutFromIni, GEngineIni);
	const int32 ClampedTimeout = FMath::Clamp(TimeoutFromIni,
		static_cast<int32>(EOS_ANTICHEATCLIENT_REGISTERPEER_MIN_AUTHENTICATIONTIMEOUT),
		static_cast<int32>(EOS_ANTICHEATCLIENT_REGISTERPEER_MAX_AUTHENTICATIONTIMEOUT));

	EOS_AntiCheatClient_RegisterPeerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_REGISTERPEER_API_LATEST;
	// Reuse the PUID as PeerHandle - PUIDs are stable for the platform lifetime.
	Options.PeerHandle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(PeerPuid);
	Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
	Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Windows;
	Options.AuthenticationTimeout = static_cast<uint32_t>(ClampedTimeout);
	Options.IpAddress = nullptr;
	Options.PeerProductUserId = PeerPuid;

	const EOS_EResult Result = EOS_AntiCheatClient_RegisterPeer(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::RegisterPeer] EOS_AntiCheatClient_RegisterPeer(%s) failed: %s"),
			*LexToString(PeerPuid), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}

	PeerByProductId.Add(PeerPuid, PeerUser);
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::RegisterPeer] Peer %s registered (auth timeout %ds)."),
		*LexToString(PeerPuid), ClampedTimeout);
	return true;
}

void FEOSAntiCheatClient::UnregisterPeer(const FUniqueNetIdRef& PeerUser)
{
	if (!Handle || !bSessionActive)
	{
		return;
	}

	const EOS_ProductUserId PeerPuid = static_cast<const IUniqueNetIdEOS&>(*PeerUser).GetProductUserId();
	if (!PeerByProductId.Remove(PeerPuid))
	{
		return;
	}

	EOS_AntiCheatClient_UnregisterPeerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_UNREGISTERPEER_API_LATEST;
	Options.PeerHandle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(PeerPuid);
	EOS_AntiCheatClient_UnregisterPeer(Handle, &Options);

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::UnregisterPeer] Peer %s unregistered."), *LexToString(PeerPuid));
}

bool FEOSAntiCheatClient::TickReceivePump(float /*DeltaSeconds*/)
{
	if (!P2PHandle || !bSessionActive)
	{
		return true;
	}

	// Drain queued packets this tick (cap=MaxPacketsPerTick). MUST pass our
	// channel - ReceivePacket(nullptr) would steal NetDriverEOS packets.
	constexpr int32 MaxPacketsPerTick = 32;
	for (int32 Drained = 0; Drained < MaxPacketsPerTick; ++Drained)
	{
		EOS_P2P_GetNextReceivedPacketSizeOptions SizeOpts = {};
		SizeOpts.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		SizeOpts.LocalUserId = LocalPuid;
		SizeOpts.RequestedChannel = &AntiCheatChannelHash;
		uint32_t PacketSize = 0;
		if (EOS_P2P_GetNextReceivedPacketSize(P2PHandle, &SizeOpts, &PacketSize) != EOS_EResult::EOS_Success || PacketSize == 0)
		{
			break;
		}

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(PacketSize);
		EOS_P2P_ReceivePacketOptions ReceiveOpts = {};
		ReceiveOpts.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
		ReceiveOpts.LocalUserId = LocalPuid;
		ReceiveOpts.MaxDataSizeBytes = PacketSize;
		ReceiveOpts.RequestedChannel = &AntiCheatChannelHash;

		EOS_ProductUserId RemotePuid = nullptr;
		EOS_P2P_SocketId OutSocket = {};
		OutSocket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		uint8_t OutChannel = 0;
		uint32_t BytesWritten = 0;
		const EOS_EResult ReceiveResult = EOS_P2P_ReceivePacket(P2PHandle, &ReceiveOpts, &RemotePuid, &OutSocket, &OutChannel, Buffer.GetData(), &BytesWritten);
		if (ReceiveResult != EOS_EResult::EOS_Success || BytesWritten == 0)
		{
			break;
		}
		if (!PeerByProductId.Contains(RemotePuid))
		{
			UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::TickReceivePump] Dropped %d bytes from unregistered peer %s."),
				BytesWritten, *LexToString(RemotePuid));
			continue;
		}

		EOS_AntiCheatClient_ReceiveMessageFromPeerOptions RmOpts = {};
		RmOpts.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMPEER_API_LATEST;
		RmOpts.PeerHandle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(RemotePuid);
		RmOpts.DataLengthBytes = BytesWritten;
		RmOpts.Data = Buffer.GetData();
		const EOS_EResult RmResult = EOS_AntiCheatClient_ReceiveMessageFromPeer(Handle, &RmOpts);
		if (RmResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::TickReceivePump] SDK rejected %d bytes from %s: %s"),
				BytesWritten, *LexToString(RemotePuid), ANSI_TO_TCHAR(EOS_EResult_ToString(RmResult)));
		}
	}
	return true;
}

void EOS_CALL FEOSAntiCheatClient::OnMessageToPeerStatic(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnMessageToPeer(*Data);
	}
}

void EOS_CALL FEOSAntiCheatClient::OnPeerActionRequiredStatic(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnPeerActionRequired(*Data);
	}
}

void EOS_CALL FEOSAntiCheatClient::OnPeerAuthStatusChangedStatic(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnPeerAuthStatusChanged(*Data);
	}
}

void EOS_CALL FEOSAntiCheatClient::OnP2PConnectionRequestStatic(const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
{
	if (Data && Data->ClientData)
	{
		static_cast<FEOSAntiCheatClient*>(Data->ClientData)->OnP2PConnectionRequest(*Data);
	}
}

void FEOSAntiCheatClient::OnMessageToPeer(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo& Info)
{
	// PeerHandle is the PUID we passed in RegisterPeer; cast back.
	const EOS_ProductUserId PeerPuid = reinterpret_cast<EOS_ProductUserId>(Info.ClientHandle);
	if (!P2PHandle || !PeerByProductId.Contains(PeerPuid))
	{
		return;
	}

	EOS_P2P_SendPacketOptions Options = {};
	Options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
	Options.LocalUserId = LocalPuid;
	Options.RemoteUserId = PeerPuid;
	Options.SocketId = &AntiCheatSocketId;
	Options.Channel = AntiCheatChannelHash;
	Options.DataLengthBytes = Info.MessageDataSizeBytes;
	Options.Data = Info.MessageData;
	Options.bAllowDelayedDelivery = EOS_TRUE;
	// EAC control messages require reliable-ordered.
	Options.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
	Options.bDisableAutoAcceptConnection = EOS_FALSE;

	const EOS_EResult Result = EOS_P2P_SendPacket(P2PHandle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::OnMessageToPeer] SendPacket(%s, %u bytes) failed: %s"),
			*LexToString(PeerPuid), Info.MessageDataSizeBytes, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogEOSAntiCheatPlugin, VeryVerbose, TEXT("[FEOSAntiCheatClient::OnMessageToPeer] local -> %s, %u bytes."),
		*LexToString(PeerPuid), Info.MessageDataSizeBytes);
#endif
}

void FEOSAntiCheatClient::OnPeerActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo& Info)
{
	if (Info.ClientAction != EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
	{
		return;
	}

	const FString Reason = DescribeActionReason(Info.ActionReasonCode, Info.ActionReasonDetailsString);
	const bool bIsSelf = Info.ClientHandle == EOS_ANTICHEATCLIENT_PEER_SELF;
	if (bIsSelf)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheatClient::OnPeerActionRequired] Local client flagged (PEER_SELF): %s"), *Reason);
		PeerActionRequiredDelegate.Broadcast(nullptr, Reason);
		return;
	}

	const EOS_ProductUserId PeerPuid = reinterpret_cast<EOS_ProductUserId>(Info.ClientHandle);
	FUniqueNetIdRef* PeerRef = PeerByProductId.Find(PeerPuid);
	UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::OnPeerActionRequired] Peer %s must be removed: %s"),
		*LexToString(PeerPuid), *Reason);
	PeerActionRequiredDelegate.Broadcast(PeerRef ? PeerRef->ToSharedPtr() : FUniqueNetIdPtr(), Reason);
}

void FEOSAntiCheatClient::OnPeerAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo& Info)
{
	if (Info.ClientHandle == EOS_ANTICHEATCLIENT_PEER_SELF)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::OnPeerAuthStatusChanged] Local auth status -> %d."),
			static_cast<int32>(Info.ClientAuthStatus));
		return;
	}
	const EOS_ProductUserId PeerPuid = reinterpret_cast<EOS_ProductUserId>(Info.ClientHandle);
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::OnPeerAuthStatusChanged] Peer %s auth status -> %d."),
		*LexToString(PeerPuid), static_cast<int32>(Info.ClientAuthStatus));
}

void FEOSAntiCheatClient::OnP2PConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo& Info)
{
	// Reject strays - only accept peers we've registered with EAC.
	if (!Info.RemoteUserId || !PeerByProductId.Contains(Info.RemoteUserId))
	{
		UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::OnP2PConnectionRequest] Rejected connection from unregistered peer %s."),
			*LexToString(Info.RemoteUserId));
		return;
	}

	EOS_P2P_AcceptConnectionOptions Options = {};
	Options.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
	Options.LocalUserId = LocalPuid;
	Options.RemoteUserId = Info.RemoteUserId;
	Options.SocketId = &AntiCheatSocketId;
	const EOS_EResult Result = EOS_P2P_AcceptConnection(P2PHandle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheatClient::OnP2PConnectionRequest] AcceptConnection(%s) failed: %s"),
			*LexToString(Info.RemoteUserId), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return;
	}
	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheatClient::OnP2PConnectionRequest] Accepted AC socket from %s."),
		*LexToString(Info.RemoteUserId));
}

#endif // P2PMODE
