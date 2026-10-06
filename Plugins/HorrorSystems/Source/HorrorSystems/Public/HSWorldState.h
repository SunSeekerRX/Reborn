#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSItemData.h"
#include "HSWorldState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHSHotbarChanged);

// Lives across OpenLevel; does not rely on actors surviving travel.
UCLASS()
class HORRORSYSTEMS_API UHSWorldState : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UHSWorldState();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health") int32 Lives=3;
    UFUNCTION(BlueprintCallable, Category="Health") bool LoseLife();
    UFUNCTION(BlueprintPure, Category="Health") bool IsDefeated() const { return Lives<=0; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hotbar") TArray<FHSItemSlot> Slots;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hotbar") int32 SelectedSlot = 0;
    UPROPERTY(BlueprintAssignable, Category="Hotbar") FHSHotbarChanged OnHotbarChanged;
    UPROPERTY() TSet<FString> CollectedPickups;
    UPROPERTY() FName PendingSpawnTag;
    UPROPERTY() bool bTravelPending = false;
    UFUNCTION(BlueprintCallable, Category="Hotbar") bool TryAddItem(UHSItemData* Item, const FString& WorldKey);
    UFUNCTION(BlueprintCallable, Category="Hotbar") bool SwapSlots(int32 From, int32 To);
    UFUNCTION(BlueprintCallable, Category="Hotbar") bool SelectSlot(int32 Index);
    UFUNCTION(BlueprintPure, Category="Hotbar") UHSItemData* GetSelectedItem() const;
    UFUNCTION(BlueprintCallable, Category="Session") void ResetSession();
};
