#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "HSAI.generated.h"
class AHSCharacter;

UENUM(BlueprintType)
enum class EHSPursuitState : uint8 { CatchUp, Approach, Waiting, Blocked };

UENUM(BlueprintType)
enum class EHSMovementState : uint8 { Slow, Normal, Fast };

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSMonster : public ACharacter
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pursuit") FName HomeRoom;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pursuit") bool bPursuitEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pursuit") bool bUseStoryPressure=false;
    AHSMonster();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Vision") TObjectPtr<class UPointLightComponent> RedAuraLight;
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat") bool bCanDamagePlayer=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0.1")) float StaggerDuration=5.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0")) float StaggerSpeed=30.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Combat") float StaggerRemaining=0.f;
    float RecoilRemaining=0.f;
    bool TryContactPlayer(AHSCharacter* Player);
private:
    bool bContactArmed=true;
    UFUNCTION() void OnCapsuleHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pursuit") bool bCinematicActor=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pursuit") bool bAutomaticSpeed=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Pursuit") EHSMovementState MovementState=EHSMovementState::Normal;
    UFUNCTION(BlueprintCallable,Category="Pursuit") void SetMovementState(EHSMovementState State,bool Automatic=false) { MovementState=State; bAutomaticSpeed=Automatic; }
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") TObjectPtr<class USoundBase> FootstepSound;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") TObjectPtr<class USoundBase> PresenceSound;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Audio") TObjectPtr<class UAudioComponent> PresenceAudio;
private:
    float StepDistance=0;
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pursuit") EHSPursuitState PursuitState=EHSPursuitState::CatchUp;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pursuit") float DistanceToPlayer=0.f;
};

UCLASS()
class HORRORSYSTEMS_API AHSPursuitController : public AAIController
{
    GENERATED_BODY()
public:
    AHSPursuitController();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pursuit") TObjectPtr<class UBehaviorTree> ChaseTree;
    UPROPERTY() TObjectPtr<AHSCharacter> Target;
    void RefreshTarget();
    void SetBlocked(bool bBlocked=true);
private:
    bool bPathBlocked=false;
};

UCLASS()
class HORRORSYSTEMS_API UBTService_HSTarget : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_HSTarget();
protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory,float Dt) override;
};

UCLASS()
class HORRORSYSTEMS_API UBTTask_HSPursue : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_HSPursue();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory,float Dt) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(float); }
};
