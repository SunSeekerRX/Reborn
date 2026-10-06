#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRoomActors.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UCameraComponent;
class UHSRoomRules;
class UHSProgression;
class AHSCharacter;
class AHSMonster;
class AHSWindowSequence;

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSRoomDirector : public AActor
{
    GENERATED_BODY()
public:
    AHSRoomDirector();
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> SafeArea;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> RoomBounds;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bUseRoomBounds=false;
    UFUNCTION(BlueprintCallable) void PrepareEntry();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> ReturnBarrier;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UHSRoomRules> Rules;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AHSWindowSequence> WindowSequence;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName SafeSpawnTag=TEXT("Safe");
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bPlayFirstEntrySequence=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bSafeAreaSealed=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float EndingTime=0;
    UFUNCTION(BlueprintCallable) void FinishIntro();
    UFUNCTION(BlueprintCallable) void HandleTimeout();
    UFUNCTION(BlueprintCallable) void BeginEnding();
    UFUNCTION(BlueprintCallable) bool ChangeStage(int32 NewStage);
    UFUNCTION(BlueprintPure) bool IsSafe(const AHSCharacter* Player) const;
    UHSProgression* Progress() const;
    static AHSRoomDirector* Find(UWorld* World);
private:
    bool bInitialized=false,bResetting=false;
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSWindowSequence : public AActor
{
    GENERATED_BODY()
public:
    AHSWindowSequence();
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraOffset=FVector(0,350,175);
    // Local-space points between the player and the window; used in both directions.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera") TArray<FVector> CameraWaypoints;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="12")) float CameraCollisionRadius=16.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FRotator WindowViewRotation=FRotator(0,-90,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector MonsterStart=FVector(-650,-300,92);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector MonsterLookPoint=FVector(0,-300,92);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector MonsterEnd=FVector(1200,-300,92);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1")) float Duration=9.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bPlaying=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Elapsed=0.f;
    UPROPERTY() TObjectPtr<AHSMonster> Performer;
    UFUNCTION(BlueprintCallable) void Play();
    UFUNCTION(BlueprintCallable) void Finish();
private:
    FVector StartView;
    FRotator StartRotation;
    TArray<FVector> CameraPath;
    TArray<float> CameraPathDistances;
    float CurrentPathDistance=0.f;
    void MoveCameraAlongPath(float Alpha,const FQuat& Rotation);
};

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSMovableProp : public AActor
{
    GENERATED_BODY()
public:
    AHSMovableProp();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector OpenOffset=FVector(0,0,80);
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FRotator OpenRotation=FRotator(0,0,0);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1")) float MoveDuration=.6f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName ClueOnOpen;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<class USoundBase> InteractionSound;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bOpen=false;
    UFUNCTION(BlueprintCallable) bool Interact(AHSCharacter* Player);
private:
    FTransform ClosedTransform;
    float Alpha=0;
};
