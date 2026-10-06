#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSWorldActors.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UAudioComponent;
class UHSItemData;
class AHSCharacter;
class UWorld;

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSPickup : public AActor
{
    GENERATED_BODY()
public:
    AHSPickup();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item") TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item") TObjectPtr<UHSItemData> ItemData;
    // Unique per actor within its level. Keep it stable when editing/moving an actor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item") FName PickupId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") FName ClueId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") int32 MinimumStage=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") int32 MaximumStage=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") int32 RequiredStoryStep=-1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item") bool bRotateForDemo=true;
    UFUNCTION(BlueprintCallable, Category="Item") bool TryPickup(AHSCharacter* Character);
    FString GetPersistentKey() const;
private:
    bool bClaimed=false;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSPortal : public AActor
{
    GENERATED_BODY()
public:
    AHSPortal();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") bool bTravelEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") bool bRequiresSafeReturn=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") bool bWhiteLightTravel=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") TObjectPtr<class AHSWoodDoor> OccludingDoor;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Portal") TObjectPtr<class UPointLightComponent> PortalLight;
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") bool bUseStageRoute=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") bool bLocalTravel=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Portal") FName LocalTargetRoom;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression") TObjectPtr<class UHSRoomRules> RoomRules;
    bool IsLocked() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal") TObjectPtr<UBoxComponent> Trigger;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal") TObjectPtr<UStaticMeshComponent> Marker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal") TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal") TSoftObjectPtr<UWorld> Destination;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal") FName DestinationSpawnTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal") FText PortalName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<class USoundBase> TravelSound;
    UFUNCTION(BlueprintCallable, Category="Portal") bool Travel(AHSCharacter* Character);
protected:
    UFUNCTION() void OnOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
    bool bUsed=false;
    float LocalCooldownUntil=0.f;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSAmbientZone : public AActor
{
    GENERATED_BODY()
public:
    AHSAmbientZone();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<UAudioComponent> Audio;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<class USoundBase> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0", ClampMax="1")) float Volume=.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio", meta=(ClampMin="100")) float AudibleRadius=1600.f;
};

// One per level: configures darkness/fog and demonstration collision primitives.
UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSVisionRig : public AActor
{
    GENERATED_BODY()
public:
    AHSVisionRig();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision") TObjectPtr<class UExponentialHeightFogComponent> Fog;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision") TObjectPtr<class UPostProcessComponent> PostProcess;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vision") bool bUseProjectSettings=true;
};
