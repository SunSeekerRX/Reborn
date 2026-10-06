#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSItemData.h"
#include "HSProgression.h"
#include "Containers/Ticker.h"
#include "HSWorldState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHSHotbarChanged);

// Lives across OpenLevel; does not rely on actors surviving travel.
UCLASS()
class HORRORSYSTEMS_API UHSWorldState : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UHSWorldState();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool BeginWhiteTravel(const FHSRoomRoute& Route,FName SpawnTag,bool bLocal,FName LocalRoom);
    UFUNCTION(BlueprintCallable,Category="Travel") bool BeginMenuTravel(bool bStartGame);
    UFUNCTION(BlueprintCallable,Category="Health") bool RecoverAtSafety();
    bool IsInSafety(const class AHSCharacter* Player) const;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="UI") bool bTitleScreen=false;
    UPROPERTY() FTransform RecoveryTransform;
    UPROPERTY() bool bHasRecovery=false;
    UPROPERTY() bool bRecoverySafety=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Travel") float WhiteTravelAlpha=0.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Travel") bool bWhiteTransition=false;
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
private:
    FTSTicker::FDelegateHandle TravelTicker;
    FDelegateHandle LoadedMapHandle;
    TSharedPtr<class SWidget> TravelOverlay;
    FHSRoomRoute PendingRoute;
    FName PendingLocalRoom;
    bool bPendingLocal=false;
    bool bPendingMenu=false;
    int32 TravelPhase=0;
    float TravelHold=0.f;
    bool TickWhiteTravel(float Dt);
    void AttachTravelOverlay();
    void OnTravelMapLoaded(UWorld* World);
};
