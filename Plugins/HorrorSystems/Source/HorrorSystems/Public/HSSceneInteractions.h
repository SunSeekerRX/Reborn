#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSSceneInteractions.generated.h"
class UStaticMeshComponent;
class UHSItemData;
class AHSCharacter;
class AHSPlayerController;

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSSceneAudio : public AActor
{
    GENERATED_BODY()
public:
    AHSSceneAudio();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UAudioComponent> Ambient;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UAudioComponent> Music;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UAudioComponent> Pressure;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") float AmbientVolume=.22f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") float MusicVolume=.30f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") float PressureVolume=.30f;
private:
    int32 LastStage=0;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSCollapsingObstacle : public AActor
{
    GENERATED_BODY()
public:
    AHSCollapsingObstacle();
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collapse") FVector FallPivotOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collapse") FVector FallAxis=FVector(0,1,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collapse") float FallAngle=90.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collapse",meta=(ClampMin=".1")) float FallDuration=1.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") TObjectPtr<class USoundBase> CollapseSound;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collapse") bool bFalling=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collapse") bool bCollapsed=false;
    UFUNCTION(BlueprintCallable) void StartCollapse();
private:
    FTransform Start;
    FVector Pivot;
    float Elapsed=0.f;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSInspectTrigger : public AActor
{
    GENERATED_BODY()
public:
    AHSInspectTrigger();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UHSItemData> ItemData;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AHSCollapsingObstacle> Obstacle;
    UFUNCTION(BlueprintCallable) bool Interact(AHSCharacter* Player);
private:
    UPROPERTY() TWeakObjectPtr<AHSPlayerController> InspectingController;
    bool bArmed=false;
    UFUNCTION() void OnInspectionClosed(UHSItemData* Item);
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSSwapPainting : public AActor
{
    GENERATED_BODY()
public:
    AHSSwapPainting();
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName SwapGroup;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1")) float SwapDuration=.9f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") TObjectPtr<class USoundBase> SwapSound;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bSwapping=false;
    bool SwapWith(AHSSwapPainting* Other);
private:
    FVector From,To;
    float Elapsed=0.f;
};
