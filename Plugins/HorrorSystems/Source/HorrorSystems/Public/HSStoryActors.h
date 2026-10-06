#pragma once
#include "CoreMinimal.h"
#include "HSRoomActors.h"
#include "HSStoryActors.generated.h"

class AHSMonster;
class UPointLightComponent;
class UHSItemData;

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSWoodDoor : public AHSMovableProp
{
    GENERATED_BODY()
public:
    AHSWoodDoor();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> PawnBlocker;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector HingeOffset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float OpenAngle=-100.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bRequiresFinalMessage=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bLocked=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float OpenAlpha=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<class USoundBase> OpenSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<class USoundBase> CloseSound;
    UFUNCTION(BlueprintCallable) bool Use(AHSCharacter* Player);
    UFUNCTION(BlueprintCallable) void CloseAndLock();
    UFUNCTION(BlueprintCallable) void Unlock();
    UFUNCTION(BlueprintPure) bool IsOpen() const {return OpenAlpha>.05f;}
    bool HasClearedFromSafety(const AHSCharacter* Player, const FVector& SafeCenter) const;
private:
    FTransform Closed;
    FTransform ClosedBlocker;
    FVector Hinge;
    float EffectiveOpenAngle=100.f;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSFlickerLamp : public AActor
{
    GENERATED_BODY()
public:
    AHSFlickerLamp();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UPointLightComponent> Light;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AActor> Fixture;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float EntryRadius=500.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float BaseIntensity=90.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bFirstEntryDoubleFlash=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEmergencyGuidance=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 EntryCount=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bInside=false;
private:
    float FlickerRemaining=0;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BulbMaterial;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSVisitPainting : public AActor
{
    GENERATED_BODY()
public:
    AHSVisitPainting();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<class UMaterialInterface> BlankMaterial;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bBlank=false;
private:
    UPROPERTY() TArray<TObjectPtr<class UMaterialInterface>> Artwork;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSStoryDirector : public AHSRoomDirector
{
    GENERATED_BODY()
public:
    AHSStoryDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AHSWoodDoor> SafeDoor;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AHSMonster> Pursuer;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<TObjectPtr<class AHSCollapsingObstacle>> EscapeObstacles;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector MonsterTriggerPoint;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float MonsterTriggerRadius=500.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bNarrativeFinished=false;
    UFUNCTION() void OnStoryInspectionClosed(UHSItemData* Item);
    FText MissionText() const;
private:
    bool bEntered=false,bMonsterTriggered=false,bVisitedSafety=false;
    float SpawnDelay=-1.f;
    TWeakObjectPtr<class AHSPlayerController> BoundController;
    void ActivateMonster(bool Active);
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSRoomVariation : public AActor
{
    GENERATED_BODY()
public:
    AHSRoomVariation();
    virtual void BeginPlay() override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AActor> Furniture;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector FurnitureOffset=FVector(-90,0,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 MinimumStage=2;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UTextRenderComponent> Writing;
};
