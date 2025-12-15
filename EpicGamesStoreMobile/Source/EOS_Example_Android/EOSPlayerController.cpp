// Copyright Epic Games, Inc. All Rights Reserved.


#include "EOSPlayerController.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineEntitlementsInterface.h"

DEFINE_LOG_CATEGORY(LogEOSOSSMobileTutorial);

void AEOSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	AutoLogin();
}

void AEOSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineEntitlementsPtr Entitlements = Subsystem ? Subsystem->GetEntitlementsInterface() : nullptr;
	if (Identity.IsValid())
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, AutoLoginDelegateHandle);
		AutoLoginDelegateHandle.Reset();
	}

	if (Entitlements.IsValid())
	{
		Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
		QueryEntitlementsDelegateHandle.Reset();
	}
}

void AEOSPlayerController::AutoLogin()
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	if (Identity.IsValid())
	{
		FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			QueryOffers();
			QueryReceipts();
			QueryEntitlements();
			OnAutoLoginComplete.Broadcast(true, TEXT(""));
			return;
		}

		AutoLoginDelegateHandle =
			Identity->AddOnLoginCompleteDelegate_Handle(
				0,
				FOnLoginCompleteDelegate::CreateUObject(
					this,
					&ThisClass::HandleAutoLoginCompleted));

		// Call AutoLogin - Plugin configuration uses PersistentAuth, the required credential type for mobile
		if (Identity->AutoLogin(0))
		{
			UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::AutoLogin] Identity AutoLogin called successfully"));
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::AutoLogin] Failed"));
			Identity->ClearOnLoginCompleteDelegate_Handle(0, AutoLoginDelegateHandle);
			AutoLoginDelegateHandle.Reset();
			OnAutoLoginComplete.Broadcast(false, TEXT("[AEOSPlayerController::AutoLogin] Failed"));
		}
	}
	else
	{
		OnAutoLoginComplete.Broadcast(false, TEXT("[AEOSPlayerController::AutoLogin] IdentityInterface null"));
	}
}

void AEOSPlayerController::HandleAutoLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::HandleAutoLoginCompleted] Success - Player %s"), *UserId.ToString());
		QueryOffers();
		QueryReceipts();
		QueryEntitlements();
	}
	else
	{
		UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::HandleAutoLoginCompleted] Failed"));
	}

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;

	if (Identity.IsValid())
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, AutoLoginDelegateHandle);
		AutoLoginDelegateHandle.Reset();
	}

	OnAutoLoginComplete.Broadcast(bWasSuccessful, Error);
}

void AEOSPlayerController::QueryOffers()
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineStoreV2Ptr Store = Subsystem ? Subsystem->GetStoreV2Interface() : nullptr;

	if (Identity.IsValid() && Store.IsValid())
	{
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);

		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			FOnlineStoreFilter EmptyStoreFilter = {};
			Store->QueryOffersByFilter(*LocalNetId, EmptyStoreFilter,
				FOnQueryOnlineStoreOffersComplete::CreateWeakLambda(this, [this](bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& Error)
					{
						if (bWasSuccessful)
						{
							UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::QueryOffers] Success"));
							
							CachedOffers.Reset();
							CachedOffers.Reserve(OfferIds.Num());
							IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
							IOnlineStoreV2Ptr Store = Subsystem ? Subsystem->GetStoreV2Interface() : nullptr;
							if (Store.IsValid())
							{
								for (const FUniqueOfferId& OfferId : OfferIds)
								{
									UE_LOG(LogEOSOSSMobileTutorial, VeryVerbose, TEXT("[AEOSPlayerController::QueryOffers] OfferId: %s"), *OfferId);


									TSharedPtr<FOnlineStoreOffer> Offer = Store->GetOffer(OfferId);
									if (Offer.IsValid())
									{
										// Cache offers
										UStoreOffer* Item = NewObject<UStoreOffer>(this);
										Item->OfferId = OfferId;
										Item->Title = Offer->Title;
										Item->Price = Offer->PriceText;

										CachedOffers.Add(Item);
									}
									else
									{
										UE_LOG(LogEOSOSSMobileTutorial, VeryVerbose, TEXT("[AEOSPlayerController::QueryOffers] GetOffer returned null offer"));
									}
								}
							}
						}
						else
						{
							UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryOffers] Failed with %s"), *Error);
						}
						OnQueryOffersComplete.Broadcast(bWasSuccessful, Error);
					}));
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryOffers] Failed - Player logged out"));
			OnQueryOffersComplete.Broadcast(false, TEXT("[AEOSPlayerController::QueryOffers] Failed - Player logged out"));
		}
	}
	else
	{
		OnQueryOffersComplete.Broadcast(false, TEXT("[AEOSPlayerController::QueryOffers] Identity and/or Store interface null"));
	}
}

void AEOSPlayerController::QueryReceipts(bool bRestoreReceipts)
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;

	if (Identity.IsValid() && Purchase.IsValid())
	{
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);

		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			Purchase->QueryReceipts(*LocalNetId, bRestoreReceipts,
				FOnQueryReceiptsComplete::CreateWeakLambda(this, [this, LocalNetId](const FOnlineError& Error)
					{
						if (Error.bSucceeded)
						{
							UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::QueryReceipts] Success"));

							if (IsValid(this))
							{
								IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
								IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;

								if (Purchase.IsValid())
								{
									TArray<FPurchaseReceipt> NewReceipts;
									Purchase->GetReceipts(*LocalNetId, NewReceipts);
									CachedOwnedItems.Reset();

									for (const FPurchaseReceipt& PurchaseReceipt : NewReceipts)
									{
										for (const FPurchaseReceipt::FReceiptOfferEntry& PurchasedOffer : PurchaseReceipt.ReceiptOffers)
										{		
											if (!PurchasedOffer.LineItems.IsEmpty())
											{
												// Cache owned items
												UStoreOwnedItem* Item = NewObject<UStoreOwnedItem>(this);
												// In the FOnlinePurchaseEOS interface there is only ever 1 line item for ReceiptOfferEntry
												Item->ItemId = PurchasedOffer.LineItems[0].UniqueId;
												CachedOwnedItems.Add(Item);
												UE_LOG(LogEOSOSSMobileTutorial, VeryVerbose, TEXT("[AEOSPlayerController::QueryReceipts] TransactionId:%s OfferId:%s"), *PurchaseReceipt.TransactionId, *PurchasedOffer.OfferId);
											}
										}
									}
								}
							}
						}
						else
						{
							UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryReceipts] Failed with %s"), *Error.ToLogString());
						}
						OnQueryReceiptsComplete.Broadcast(Error.bSucceeded, Error.ToLogString());
					}));
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryReceipts] Failed - Player logged out"));
			OnQueryReceiptsComplete.Broadcast(false, TEXT("[AEOSPlayerController::QueryReceipts] Failed - Player logged out"));
		}
	}
	else
	{
		OnQueryReceiptsComplete.Broadcast(false, TEXT("[AEOSPlayerController::QueryReceipts] Identity and/or Purchase interface null"));
	}

}

void AEOSPlayerController::QueryEntitlements()
{
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlineEntitlementsPtr Entitlements = Subsystem ? Subsystem->GetEntitlementsInterface() : nullptr;

	if (Identity.IsValid() && Entitlements.IsValid())
	{
		FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);
		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			QueryEntitlementsDelegateHandle =
				Entitlements->AddOnQueryEntitlementsCompleteDelegate_Handle(
					FOnQueryEntitlementsCompleteDelegate::CreateUObject(
						this,
						&ThisClass::HandleQueryEntitlementsCompleted));

			// Call QueryEntitlements with empty namespace - namespaces in EOS are only used on PC legacy flow
			if (Entitlements->QueryEntitlements(*LocalNetId, TEXT("")))
			{
				UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::QueryEntitlements] Entitlements QueryEntitlements called successfully"));
			}
			else
			{
				UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryEntitlements] Failed"));
				Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
				QueryEntitlementsDelegateHandle.Reset();
				OnQueryEntitlementsComplete.Broadcast(false, TEXT("Query Entitlements failed"));
			}
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::QueryEntitlements] Failed - Player logged out"));
			OnQueryEntitlementsComplete.Broadcast(false, TEXT("Player logged out"));
			
		}
	}
	else
	{
		OnQueryEntitlementsComplete.Broadcast(false, TEXT("[AEOSPlayerController::QueryEntitlements] Identity and/or Entitlements interface null"));
	}
}

void AEOSPlayerController::HandleQueryEntitlementsCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Namespace, const FString& Error)
{
	//Namespace is not used
	(void)Namespace;

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineEntitlementsPtr Entitlements = Subsystem ? Subsystem->GetEntitlementsInterface() : nullptr;

	if (Entitlements.IsValid())
	{
		Entitlements->ClearOnQueryEntitlementsCompleteDelegate_Handle(QueryEntitlementsDelegateHandle);
		QueryEntitlementsDelegateHandle.Reset();

		if (bWasSuccessful)
		{
			UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Success"));

			TArray<TSharedRef<FOnlineEntitlement>> NewEntitlements;
			Entitlements->GetAllEntitlements(UserId, TEXT(""), NewEntitlements);
			CachedEntitlements.Reset();
			CachedEntitlements.Reserve(NewEntitlements.Num());

			for (const TSharedRef<FOnlineEntitlement>& NewEntitlement : NewEntitlements)
			{
				UE_LOG(LogEOSOSSMobileTutorial, VeryVerbose, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] EntitlementId: %s"), *NewEntitlement->Id);

				UStoreEntitlement* Entitlement = NewObject<UStoreEntitlement>(this);
				Entitlement->Id = NewEntitlement->Id;
				Entitlement->Name = NewEntitlement->Name;
				Entitlement->ItemId = NewEntitlement->ItemId;
				Entitlement->bIsConsumable = NewEntitlement->bIsConsumable;
				Entitlement->ConsumedCount = NewEntitlement->ConsumedCount;
				Entitlement->EndDate = NewEntitlement->EndDate;

				CachedEntitlements.Add(Entitlement);
			}

			OnQueryEntitlementsComplete.Broadcast(bWasSuccessful, Error);
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Failed with %s"), *Error);
			OnQueryEntitlementsComplete.Broadcast(bWasSuccessful, Error);
		}
	}
	else
	{
		UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::HandleQueryEntitlementsCompleted] Entitlements interface null"));
		OnQueryEntitlementsComplete.Broadcast(false, Error);
	}	
}

void AEOSPlayerController::CheckoutOffers(const TArray<UStoreOffer*>& OffersToCheckout)
{
	// Assumption here is the checkout call is only coming from the UI - no deduplication here
	FPurchaseCheckoutRequest CheckoutRequest = {};

	for (const UStoreOffer* OfferToCheckoutInfo : OffersToCheckout)
	{
		// EOS doesn't use namespace
		CheckoutRequest.AddPurchaseOffer("", OfferToCheckoutInfo->OfferId, OfferToCheckoutInfo->Quantity, OfferToCheckoutInfo->bConsumable);
	}
	
	if (CheckoutRequest.PurchaseOffers.IsEmpty())
	{
		UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::Checkout] CheckoutRequest IsEmpty"));
		OnCheckoutComplete.Broadcast(false, TEXT("[AEOSPlayerController::Checkout] CheckoutRequest IsEmpty"));
		return;
	}

	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;

	if (Identity.IsValid() && Purchase.IsValid())
	{
		const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);

		if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
		{
			Purchase->Checkout(*LocalNetId, CheckoutRequest,
				FOnPurchaseCheckoutComplete::CreateWeakLambda(this, [this](const FOnlineError& Error, const TSharedRef<FPurchaseReceipt>& Receipt)
					{
						if (Error.bSucceeded)
						{
							if (Receipt->ReceiptOffers.Num() == 0)
							{
								UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::Checkout] Success, but empty receipt"));
							}
							else
							{
								UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::Checkout] Success"));
								for (const auto& PurchasedOffer : Receipt->ReceiptOffers)
								{
									UE_LOG(LogEOSOSSMobileTutorial, VeryVerbose, TEXT("[AEOSPlayerController::Checkout] TransactionId:%s OfferId:%s"), *Receipt->TransactionId, *PurchasedOffer.OfferId);
								}
							}
							QueryEntitlements();
						}
						else
						{
							UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::Checkout] Failed with %s"), *Error.ToLogString());
						}
						OnCheckoutComplete.Broadcast(Error.bSucceeded, Error.ToLogString());
					}));
		}
		else
		{
			UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::Checkout] Failed - Player logged out"));
			OnCheckoutComplete.Broadcast(false, TEXT("[AEOSPlayerController::Checkout] Failed - Player logged out"));
		}
	}
	else
	{
		OnCheckoutComplete.Broadcast(false, TEXT("[AEOSPlayerController::Checkout] Identity and/or Purchase interface null"));
	}
}

void AEOSPlayerController::RedeemEntitlement(FString EntitlementId)
{
	// Redeeming entitlements should only be done on a trusted server to avoid piracy.
	// Redeeming entitlements should only be done AFTER items have been granted to the player
	if (IsRunningDedicatedServer())
	{
		IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
		IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
		IOnlinePurchasePtr Purchase = Subsystem ? Subsystem->GetPurchaseInterface() : nullptr;

		if (Identity.IsValid() && Purchase.IsValid())
		{
			const FUniqueNetIdPtr LocalNetId = Identity->GetUniquePlayerId(0);

			if (LocalNetId.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::Type::LoggedIn)
			{
				Purchase->FinalizeReceiptValidationInfo(*LocalNetId, EntitlementId,
					FOnFinalizeReceiptValidationInfoComplete::CreateWeakLambda(this, [this, LocalNetId](const FOnlineError& Error, const FString& ValidationInfo)
						{
							if (Error.bSucceeded)
							{
								UE_LOG(LogEOSOSSMobileTutorial, Verbose, TEXT("[AEOSPlayerController::RedeemEntitlement] Success"));
							}
							else
							{
								UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Failed with %s"), *Error.ToLogString());
							}
						}));
			}
			else
			{
				UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Failed - Player logged out"));
			}
		}
		else
		{
			OnQueryReceiptsComplete.Broadcast(false, TEXT("[AEOSPlayerController::RedeemEntitlement] Identity and/or Purchase interface null"));
		}
	}
	else
	{
		UE_LOG(LogEOSOSSMobileTutorial, Error, TEXT("[AEOSPlayerController::RedeemEntitlement] Should not be called on client"));
	}
}