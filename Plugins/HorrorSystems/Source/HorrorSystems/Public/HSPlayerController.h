#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HSPlayerController.generated.h"

class UHSItemData;
class UHSWorldState;
class SHSOverlay;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSInspectionClosed,UHSItemData*,Item);
UCLASS()
class HORRORSYSTEMS_API AHSPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AHSPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    UFUNCTION(BlueprintCallable, Category="UI") void ToggleHotbarMouse();
    UFUNCTION(BlueprintCallable, Category="UI") void InspectSelected();
    UFUNCTION(BlueprintCallable, Category="UI") void InspectItem(UHSItemData* Item);
    UFUNCTION(BlueprintCallable, Category="UI") void CloseInspection();
    UFUNCTION(BlueprintCallable, Category="UI") void SelectSlot(int32 Index);
    UFUNCTION(BlueprintCallable, Category="UI") void ActivateNumberSlot(int32 Index);
    UFUNCTION(BlueprintPure, Category="UI") UHSWorldState* GetSession() const;
    void Notify(const FText& Message, float Seconds=3.f);
    FText GetNotification() const;
    UFUNCTION(BlueprintCallable,Category="Story") void Speak(const FText& Text,float Seconds=5.f);
    FText GetSubtitle() const;
    bool IsMouseMode() const;
    UPROPERTY() TObjectPtr<class UTexture2D> TitleTexture;
    bool IsInspecting() const { return bInspecting; }
    UHSItemData* GetInspectionItem() const { return InspectionItem; }
    void Escape();
    UPROPERTY(BlueprintAssignable,Category="UI") FHSInspectionClosed OnInspectionClosed;
    UFUNCTION(BlueprintCallable,Category="Progression") void SetGameplayLocked(bool Locked);
    bool IsGameplayLocked() const { return bGameplayLocked; }
protected:
    void ApplyInputMode();
    UPROPERTY() TObjectPtr<UHSItemData> InspectionItem;
    bool bHotbarMouse=false;
    bool bInspecting=false;
    bool bOwnsPause=false;
    bool bInspectionOwnerNoSee=false;
    bool bGameplayLocked=false;
    FText Notification;
    double NotificationUntil=0;
    FText Subtitle;
    double SubtitleUntil=0;
    UPROPERTY() TObjectPtr<class UAudioComponent> TitleMusic;
    int32 LastNumberSlot=INDEX_NONE;
    double LastNumberTime=-1;
    TSharedPtr<SHSOverlay> Overlay;
};
