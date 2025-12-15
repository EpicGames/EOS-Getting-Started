// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EOSPlayerController.generated.h"

/** New log category for this tutorial */
DECLARE_LOG_CATEGORY_EXTERN(LogEOSOSSMobileTutorial, Log, All);

/** Delegate type that will be used to fire OSS async calls */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOSSAsyncResult, bool, bWasSuccessful, const FString&, Error);

/** EGS offer data - Exposed to Blueprints */
UCLASS(BlueprintType)
class UStoreOffer : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) FString OfferId;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Price;
	UPROPERTY(BlueprintReadOnly) bool   bConsumable = true;
	UPROPERTY(BlueprintReadWrite) int32  Quantity = 0;

	UStoreOffer() : OfferId{}, Title{}, Price{}, bConsumable(true), Quantity(0)
	{
	}

	UStoreOffer(const FString& InOfferId, const FText& InTitle, const FText& InPrice, bool bInConsumable, int32 InQuantity) : OfferId(InOfferId), Title(InTitle), Price(InPrice), bConsumable(bInConsumable), Quantity(InQuantity)
	{
	}
};

/** CatalogItemID owned by player - Exposed to Blueprints */
UCLASS(BlueprintType)
class UStoreOwnedItem : public UObject
{
	GENERATED_BODY()
public:
	/**  This maps to EOS_Ecom_CatalogItemId -  The item the player owns */
	UPROPERTY(BlueprintReadOnly) FString ItemId; 

	UStoreOwnedItem() : ItemId{}
	{
	}

	UStoreOwnedItem(const FString& InItemId) : ItemId(InItemId)
	{
	}
};

/** Player entitlement - Exposed to Blueprints */
UCLASS(BlueprintType)
class UStoreEntitlement : public UObject
{
	GENERATED_BODY()
public:
	/**  This maps to EOS_Ecom_EntitlementId - This is the ID to pass in FOnlinePurchaseEOS::FinalizeReceiptValidationInfo to redeem an entitlement */
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	/**  This maps to EOS_Ecom_CatalogItemId - The catalog item the player is entitled to */
	UPROPERTY(BlueprintReadOnly) FString ItemId;
	UPROPERTY(BlueprintReadOnly) bool bIsConsumable;
	/** ConsumedCount in OnlineSubsystemEOS is used to track if an entitlement has been redeemed */
	UPROPERTY(BlueprintReadWrite) int32  ConsumedCount;
	UPROPERTY(BlueprintReadWrite) FString  EndDate;

	UStoreEntitlement() : Id{}, Name{}, ItemId{}, bIsConsumable(false), ConsumedCount(0), EndDate{}
	{
	}

	UStoreEntitlement(const FString& InId, const FString& InName,  const FString& InItemId, const bool bInIsConsumable, const int32 InConsumedCount, const FString& InEndDate ) : 
		Id(InId), Name(InName), ItemId(InItemId), bIsConsumable(bInIsConsumable), ConsumedCount(InConsumedCount), EndDate(InEndDate)
	{
	}
};

/**
 * 
 */
UCLASS()
class AEOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Retrieve Epic Games Store offers (CatalogOffers) and cache them - Exposed to Blueprints */
	UFUNCTION(BlueprintCallable, Category = "Store")
	void QueryOffers();

	/** Callback when QueryOffers completes - Exposed to Blueprints */
	UPROPERTY(BlueprintAssignable, Category = "Store")
	FOnOSSAsyncResult OnQueryOffersComplete;

	/** Retrieve player owned CatalogItems and cache them - Exposed to Blueprints */
	UFUNCTION(BlueprintCallable, Category = "Store")
	void QueryReceipts(bool bRestoreReceipts = false);

	/** Callback when QueryReceipts completes - Exposed to Blueprints */
	UPROPERTY(BlueprintAssignable, Category = "Store")
	FOnOSSAsyncResult OnQueryReceiptsComplete;

	/** Retrieve player entitlements and cache them - Exposed to Blueprints */
	UFUNCTION(BlueprintCallable, Category = "Store")
	void QueryEntitlements();

	/** Callback when QueryEntitlements completes - Exposed to Blueprints */
	UPROPERTY(BlueprintAssignable, Category = "Store")
	FOnOSSAsyncResult OnQueryEntitlementsComplete;

	/** Checkout offers - Exposed to Blueprints - Note: Checkout is performed in the EOS SDK Overlay and isn't supported in-editor -  Test in a standalone game */
	UFUNCTION(BlueprintCallable, Category = "Store", meta = (AutoCreateRefTerm = "OffersToCheckout"))
	void CheckoutOffers(const TArray<UStoreOffer*>& OffersToCheckout);

	/** Callback when Checkout completes - Exposed to Blueprints */
	UPROPERTY(BlueprintAssignable, Category = "Store")
	FOnOSSAsyncResult OnCheckoutComplete;

	/** Retrieve Epic Games Store offers (CatalogOffers) from cache - Exposed to Blueprints */
	UFUNCTION(BlueprintPure, Category = "Store")
	void GetCachedOffers(TArray<UStoreOffer*>& Out) const { Out = CachedOffers; }

	/** Retrieve player owned CatalogItems from cache - Exposed to Blueprints */
	UFUNCTION(BlueprintPure, Category = "Store")
	void GetCachedOwnedItems(TArray<UStoreOwnedItem*>& Out) const { Out = CachedOwnedItems; }
	
	/** Retrieve player entitlements from cache - Exposed to Blueprints */
	UFUNCTION(BlueprintPure, Category = "Store")
	void GetCachedEntitlements(TArray<UStoreEntitlement*>& Out) const { Out = CachedEntitlements; }

	/** Cached CatalogOffers - Exposed to Blueprints */
	UPROPERTY()
	TArray<UStoreOffer*> CachedOffers;

	/** Cached CatalogItemIds - Exposed to Blueprints */
	UPROPERTY()
	TArray<UStoreOwnedItem*> CachedOwnedItems;

	/** Cached Entitlements - Exposed to Blueprints */
	UPROPERTY()
	TArray<UStoreEntitlement*> CachedEntitlements;

	/** Callback when AutoLogin completes - Exposed to Blueprints */
	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FOnOSSAsyncResult OnAutoLoginComplete;

protected:
	/** Override BeginPlay from base class to call AutoLogin  - Already callable from Blueprint */
	virtual void BeginPlay() override;

	/** Override EndPlay from base class to clear delegate */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Login to EAS and EOS - Config will default to use PersistentAuth - Offers, owned CatalogItemIds and Entitlements will be cached on successful login - Callable from Blueprint */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void AutoLogin();

	/** Delegate to bind AutoLogin callback to callback function - Not accessible from Blueprint */
	FDelegateHandle AutoLoginDelegateHandle;

	/**  Callback function for AutoLogin - Not callable from Blueprint */
	void HandleAutoLoginCompleted(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	/**  Delegate to bind QueryEntitlements callback to callback function - Not accessible from Blueprint */
	FDelegateHandle QueryEntitlementsDelegateHandle;

	/**  Callback function for QueryEntitlements - Not callable from Blueprint */
	void HandleQueryEntitlementsCompleted(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Namespace, const FString& Error);

	/** Redeem an entitlement - should only be called on a trusted server AFTER items have been granted to the player */
	void RedeemEntitlement(FString EntitlementId);
};
