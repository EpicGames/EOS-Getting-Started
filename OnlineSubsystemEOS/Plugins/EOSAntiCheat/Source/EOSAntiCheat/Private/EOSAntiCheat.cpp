// Copyright Epic Games, Inc. All Rights Reserved.

#include "IEOSAntiCheat.h"
#if !P2PMODE
#include "EOSAntiCheatServer.h"
#endif
#include "EOSAntiCheatClient.h"

#include "Modules/ModuleManager.h"

#if !WITH_EDITOR
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"
#include "Interfaces/OnlineIdentityInterface.h"  // CreateUniquePlayerId for verified-PUID remap.
#include "OnlineSubsystemEOSTypesPublic.h"        // IUniqueNetIdEOS - pulls PUID from caller-supplied FUniqueNetIds.
#include "EOSShared.h"                             // LexToString(EOS_ProductUserId) + ProductUserIdFromUtf8String.

#include "eos_sdk.h"
#include "eos_connect.h"                           // EOS_Connect_CopyIdToken, VerifyIdToken.
#endif

DEFINE_LOG_CATEGORY(LogEOSAntiCheatPlugin);

/**
 * Plugin module + IEOSAntiCheat singleton in one. Keeping them merged (rather
 * than a separate FEOSAntiCheatModule that allocates the IEOSAntiCheat impl)
 * means callers can reach the singleton with a single module lookup and the
 * lifetime is tied to the module's load/unload, which is what we want.
 */
class FEOSAntiCheat final : public IEOSAntiCheat
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IEOSAntiCheat
#if !P2PMODE
	virtual IEOSAntiCheatServer* GetServer() override { return Server.Get(); }
#endif
	virtual IEOSAntiCheatClient* GetClient() override { return Client.Get(); }
	virtual FOnAntiCheatViolation& OnViolation() override { return ViolationDelegate; }
	virtual bool CopyLocalIdToken(const FUniqueNetIdRef& LocalUser, FString& OutJsonWebToken) override;
	virtual void VerifyIdToken(const FUniqueNetIdRef& ClaimedUser, const FString& JsonWebToken, FOnIdTokenVerified OnCompleted) override;

private:
#if !P2PMODE
	TUniquePtr<FEOSAntiCheatServer> Server;
#endif
	TUniquePtr<FEOSAntiCheatClient> Client;
	FOnAntiCheatViolation ViolationDelegate;

#if !WITH_EDITOR
	// Held so the platform stays alive while this module holds SDK handles
	// through it. Lifetime mirrors the module; released in ShutdownModule.
	IEOSPlatformHandlePtr PlatformHandle;

	// Pending verify delegates indexed by the C SDK callback's ClientData - lets
	// us dispatch back to the right caller when the async verify completes.
	static void EOS_CALL OnVerifyIdTokenStatic(const EOS_Connect_VerifyIdTokenCallbackInfo* Data);
#endif
};

IEOSAntiCheat* IEOSAntiCheat::Get()
{
	return FModuleManager::GetModulePtr<IEOSAntiCheat>(TEXT("EOSAntiCheat"));
}

void FEOSAntiCheat::StartupModule()
{
#if WITH_EDITOR
	// Editor (or any Editor-target binary, including "Standalone Game" launched
	// from the editor) must not initialize AntiCheat - the SDK only runs under
	// the EasyAntiCheat bootstrapper, which only wraps packaged Game builds.
	// Logged once at module load so it's obvious why no BeginSession output follows.
	UE_LOG(LogEOSAntiCheatPlugin, Log, TEXT("[FEOSAntiCheat::StartupModule] Editor build - AntiCheat disabled. Package the game and launch via the bootstrapper to exercise the full flow."));
#endif

#if !WITH_EDITOR
	// Borrow the already-initialized EOS platform from OnlineSubsystemEOS rather
	// than standing up a second one. Same type-guarded pattern the voice-chat
	// code uses before casting to IOnlineSubsystemEOS - voice fails silently
	// without EOS as the active OSS, and AntiCheat would do the same.
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	IOnlineSubsystemEOS* EOSSubsystem = (Subsystem && Subsystem->GetSubsystemName() == EOS_SUBSYSTEM) ? static_cast<IOnlineSubsystemEOS*>(Subsystem) : nullptr;
	PlatformHandle = EOSSubsystem ? EOSSubsystem->GetEOSPlatformHandle() : nullptr;

	if (!PlatformHandle.IsValid())
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheat::StartupModule] EOS platform handle unavailable - AntiCheat will not initialize. Check that OnlineSubsystemEOS is the active OSS and its artifact credentials are populated."));
		return;
	}

	EOS_HPlatform Platform = *PlatformHandle;
	if (!Platform)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Error, TEXT("[FEOSAntiCheat::StartupModule] Platform handle dereferenced to null - AntiCheat will not initialize."));
		PlatformHandle.Reset();
		return;
	}

#if !P2PMODE
	// The server wrapper needs the module's violation delegate so it can broadcast
	// from inside its own callbacks without a separate IEOSAntiCheat::Get() lookup.
	Server = MakeUnique<FEOSAntiCheatServer>(Platform, ViolationDelegate);
#endif

	// No client-side AntiCheat interface on dedicated servers - skip the wrapper.
	if (!IsRunningDedicatedServer())
	{
		Client = MakeUnique<FEOSAntiCheatClient>(Platform);
	}

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheat::StartupModule] EOS AntiCheat plugin initialized."));
#endif
}

void FEOSAntiCheat::ShutdownModule()
{
#if !WITH_EDITOR
	// Reverse order: client/server wrappers first (they may hold SDK handles
	// derived from the platform), then release the platform reference itself.
	Client.Reset();
#if !P2PMODE
	Server.Reset();
#endif
	PlatformHandle.Reset();

	UE_LOG(LogEOSAntiCheatPlugin, Verbose, TEXT("[FEOSAntiCheat::ShutdownModule] EOS AntiCheat plugin shut down."));
#endif
}

bool FEOSAntiCheat::CopyLocalIdToken(const FUniqueNetIdRef& LocalUser, FString& OutJsonWebToken)
{
#if WITH_EDITOR
	return false;
#else
	if (!PlatformHandle.IsValid()) { return false; }
	EOS_HConnect Connect = EOS_Platform_GetConnectInterface(*PlatformHandle);
	if (!Connect) { return false; }

	const EOS_ProductUserId Puid = static_cast<const IUniqueNetIdEOS&>(*LocalUser).GetProductUserId();
	if (!EOS_ProductUserId_IsValid(Puid)) { return false; }

	EOS_Connect_CopyIdTokenOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_COPYIDTOKEN_API_LATEST;
	Options.LocalUserId = Puid;

	EOS_Connect_IdToken* Token = nullptr;
	const EOS_EResult Result = EOS_Connect_CopyIdToken(Connect, &Options, &Token);
	if (Result != EOS_EResult::EOS_Success || !Token || !Token->JsonWebToken)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheat::CopyLocalIdToken] EOS_Connect_CopyIdToken failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		if (Token) { EOS_Connect_IdToken_Release(Token); }
		return false;
	}

	OutJsonWebToken = UTF8_TO_TCHAR(Token->JsonWebToken);
	EOS_Connect_IdToken_Release(Token);
	return true;
#endif
}

#if !WITH_EDITOR
// Heap-allocated pending-verify state. Lives until the async SDK callback fires,
// then self-deletes after invoking the caller's FOnIdTokenVerified.
struct FPendingIdTokenVerify
{
	IEOSAntiCheat::FOnIdTokenVerified Delegate;
};
#endif

void FEOSAntiCheat::VerifyIdToken(const FUniqueNetIdRef& ClaimedUser, const FString& JsonWebToken, FOnIdTokenVerified OnCompleted)
{
#if WITH_EDITOR
	OnCompleted.ExecuteIfBound(false);
#else
	if (!PlatformHandle.IsValid())
	{
		OnCompleted.ExecuteIfBound(false);
		return;
	}
	EOS_HConnect Connect = EOS_Platform_GetConnectInterface(*PlatformHandle);
	if (!Connect)
	{
		OnCompleted.ExecuteIfBound(false);
		return;
	}

	// SDK requires BOTH the claimed PUID and the JWT; it confirms the token
	// signature matches the PUID. Supplying nullptr for ProductUserId makes
	// the call fail with EOS_InvalidParameters.
	const EOS_ProductUserId ClaimedPuid = static_cast<const IUniqueNetIdEOS&>(*ClaimedUser).GetProductUserId();
	const FTCHARToUTF8 JwtUtf8(*JsonWebToken);

	EOS_Connect_IdToken Token = {};
	Token.ApiVersion = EOS_CONNECT_IDTOKEN_API_LATEST;
	Token.ProductUserId = ClaimedPuid;
	Token.JsonWebToken = JwtUtf8.Get();

	EOS_Connect_VerifyIdTokenOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_VERIFYIDTOKEN_API_LATEST;
	Options.IdToken = &Token;

	auto* Pending = new FPendingIdTokenVerify{ OnCompleted };
	EOS_Connect_VerifyIdToken(Connect, &Options, Pending, &FEOSAntiCheat::OnVerifyIdTokenStatic);
#endif
}

#if !WITH_EDITOR
void EOS_CALL FEOSAntiCheat::OnVerifyIdTokenStatic(const EOS_Connect_VerifyIdTokenCallbackInfo* Data)
{
	if (!Data) { return; }
	TUniquePtr<FPendingIdTokenVerify> Pending(static_cast<FPendingIdTokenVerify*>(Data->ClientData));
	if (!Pending) { return; }

	const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
	if (!bSuccess)
	{
		UE_LOG(LogEOSAntiCheatPlugin, Warning, TEXT("[FEOSAntiCheat::OnVerifyIdTokenStatic] VerifyIdToken failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
	}
	Pending->Delegate.ExecuteIfBound(bSuccess);
}
#endif

IMPLEMENT_MODULE(FEOSAntiCheat, EOSAntiCheat)
