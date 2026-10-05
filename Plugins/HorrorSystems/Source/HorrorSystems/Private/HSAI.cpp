#include "HSAI.h"
#include "HSCharacter.h"
#include "HSAnimInstance.h"
#include "HSSettings.h"
#include "HSProgression.h"
#include "Engine/GameInstance.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "HSPolicy.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/PathFollowingComponent.h"

AHSMonster::AHSMonster()
{
    PrimaryActorTick.bCanEverTick=true;
    PresenceAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("MonsterPresence")); PresenceAudio->SetupAttachment(RootComponent);
    PresenceAudio->bAutoActivate=false; PresenceAudio->bOverrideAttenuation=true;
    PresenceAudio->AttenuationOverrides.bAttenuate=true;
    PresenceAudio->AttenuationOverrides.AttenuationShapeExtents=FVector(200,0,0); PresenceAudio->AttenuationOverrides.FalloffDistance=900;
    GetCapsuleComponent()->InitCapsuleSize(38,92);
    GetMesh()->SetRelativeLocation(FVector(0,0,-92)); GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AIControllerClass=AHSPursuitController::StaticClass(); AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw=false;
    GetCharacterMovement()->bOrientRotationToMovement=true;
    GetCharacterMovement()->RotationRate=FRotator(0,240,0);
    GetCharacterMovement()->MaxWalkSpeed=160;
    GetCharacterMovement()->MaxAcceleration=650;
    GetCharacterMovement()->BrakingDecelerationWalking=600;
}
void AHSMonster::BeginPlay()
{
    Super::BeginPlay();
    if(!FootstepSound) FootstepSound=LoadObject<USoundBase>(nullptr,TEXT("/HorrorSystems/Audio/S_MonsterStep.S_MonsterStep"));
    if(!PresenceSound) PresenceSound=LoadObject<USoundBase>(nullptr,TEXT("/HorrorSystems/Audio/S_MonsterPresence.S_MonsterPresence"));
    PresenceAudio->SetSound(PresenceSound); PresenceAudio->SetVolumeMultiplier(.22f); PresenceAudio->Play();
    if(!GetMesh()->GetSkeletalMeshAsset()) GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    if(!GetMesh()->GetAnimClass()) GetMesh()->SetAnimInstanceClass(UHSAnimInstance::StaticClass());
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/HorrorSystems/Materials/M_Monster.M_Monster")))
        for(int32 I=0;I<GetMesh()->GetNumMaterials();++I) GetMesh()->SetMaterial(I,Material);
}
void AHSMonster::Tick(float Dt)
{
    Super::Tick(Dt);
    const float Speed=GetVelocity().Size2D();
    if(Speed>30)
    {
        StepDistance+=Speed*Dt;
        if(StepDistance>170.f)
        { StepDistance=0; if(FootstepSound) { FSoundAttenuationSettings Attenuation; Attenuation.bAttenuate=true; Attenuation.AttenuationShapeExtents=FVector(150,0,0); Attenuation.FalloffDistance=1200; if(auto* Audio=UGameplayStatics::SpawnSoundAtLocation(this,FootstepSound,GetActorLocation(),FRotator::ZeroRotator,.7f,Speed>400?1.15f:.8f)) Audio->AdjustAttenuation(Attenuation); } }
    }
    else StepDistance=0;
}
AHSPursuitController::AHSPursuitController() { PrimaryActorTick.bCanEverTick=true; }
void AHSPursuitController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if(!ChaseTree)
    {
        ChaseTree=NewObject<UBehaviorTree>(this,TEXT("BT_HorrorPursuit"));
        auto* Board=NewObject<UBlackboardData>(ChaseTree,TEXT("BB_HorrorPursuit"));
        FBlackboardEntry TargetEntry; TargetEntry.EntryName=TEXT("Target");
        auto* ObjectKey=NewObject<UBlackboardKeyType_Object>(Board); ObjectKey->BaseClass=AActor::StaticClass();
        TargetEntry.KeyType=ObjectKey; Board->Keys.Add(TargetEntry);
        FBlackboardEntry DistanceEntry; DistanceEntry.EntryName=TEXT("Distance"); DistanceEntry.KeyType=NewObject<UBlackboardKeyType_Float>(Board); Board->Keys.Add(DistanceEntry);
        FBlackboardEntry StateEntry; StateEntry.EntryName=TEXT("State"); StateEntry.KeyType=NewObject<UBlackboardKeyType_Int>(Board); Board->Keys.Add(StateEntry);
        ChaseTree->BlackboardAsset=Board;
        auto* Root=NewObject<UBTComposite_Sequence>(ChaseTree,TEXT("PursuitSequence"));
        Root->Services.Add(NewObject<UBTService_HSTarget>(Root));
        FBTCompositeChild Child; Child.ChildTask=NewObject<UBTTask_HSPursue>(Root); Root->Children.Add(Child);
        ChaseTree->RootNode=Root;
    }
    UBlackboardComponent* Board=nullptr;
    UseBlackboard(ChaseTree->BlackboardAsset,Board);
    RefreshTarget(); RunBehaviorTree(ChaseTree);
}
void AHSPursuitController::RefreshTarget()
{
    Target=Cast<AHSCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(GetBlackboardComponent()) GetBlackboardComponent()->SetValueAsObject(TEXT("Target"),Target);
}
void AHSPursuitController::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Monster=Cast<AHSMonster>(GetPawn());
    if(Monster && Monster->bCinematicActor) return;
    const auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    if(!Monster || !IsValid(Target) || Monster->bCinematicActor || Progress->bCinematic || Progress->bCompleted || (!Progress->ActiveRoom.IsNone() && !Progress->bTimerRunning)) { StopMovement(); return; }
    const auto* S=GetDefault<UHSSettings>();
    const float Distance=FVector::Dist2D(Target->GetActorLocation(),Monster->GetActorLocation());
    Monster->DistanceToPlayer=Distance;
    const float Resume=FMath::Max(S->ResumeRadius,S->StopRadius+20.f);
    if(Distance<=S->StopRadius || (Monster->PursuitState==EHSPursuitState::Waiting && Distance<Resume))
    {
        Monster->PursuitState=EHSPursuitState::Waiting;
        StopMovement();
    }
    else
    {
        float Desired=HSPolicy::ChaseSpeed(Distance,S->NearRadius,S->FarRadius,S->NearSpeed,S->FarSpeed);
        // If actually visible in the player's view, avoid a fast rush into the lit area.
        auto* PC=UGameplayStatics::GetPlayerController(this,0);
        FVector2D Screen; int32 W=0,H=0;
        if(PC) PC->GetViewportSize(W,H);
        const bool Visible=PC && Distance<S->HiddenRadius && PC->ProjectWorldLocationToScreen(Monster->GetActorLocation(),Screen)
            && Screen.X>=0 && Screen.Y>=0 && Screen.X<W && Screen.Y<H && PC->LineOfSightTo(Monster);
        if(Monster->bAutomaticSpeed) Monster->MovementState=Visible || Distance<=S->NearRadius ? EHSMovementState::Slow : Distance>=S->FarRadius ? EHSMovementState::Fast : EHSMovementState::Normal;
        Desired=Monster->MovementState==EHSMovementState::Slow?S->NearSpeed:Monster->MovementState==EHSMovementState::Fast?S->FarSpeed:S->NormalSpeed;
        auto* Move=Monster->GetCharacterMovement();
        Move->MaxWalkSpeed=FMath::FInterpConstantTo(Move->MaxWalkSpeed,Desired,Dt,650.f);
        Monster->PursuitState=bPathBlocked ? EHSPursuitState::Blocked : Distance>S->FarRadius && !Visible ? EHSPursuitState::CatchUp : EHSPursuitState::Approach;
    }
    if(auto* BB=GetBlackboardComponent())
    { BB->SetValueAsFloat(TEXT("Distance"),Distance); BB->SetValueAsInt(TEXT("State"),int32(Monster->PursuitState)); }
}
void AHSPursuitController::SetBlocked(bool bBlocked)
{ bPathBlocked=bBlocked; if(bBlocked) if(auto* M=Cast<AHSMonster>(GetPawn())) M->PursuitState=EHSPursuitState::Blocked; }
UBTService_HSTarget::UBTService_HSTarget()
{ NodeName=TEXT("Refresh player target (0.10s)"); Interval=.1f; RandomDeviation=0.f; bNotifyTick=true; }
void UBTService_HSTarget::TickNode(UBehaviorTreeComponent& Owner,uint8* Memory,float Dt)
{ Super::TickNode(Owner,Memory,Dt); if(auto* AI=Cast<AHSPursuitController>(Owner.GetAIOwner())) AI->RefreshTarget(); }
UBTTask_HSPursue::UBTTask_HSPursue() { NodeName=TEXT("Pursue / near approach / wait (NavMesh)"); bNotifyTick=true; }
EBTNodeResult::Type UBTTask_HSPursue::ExecuteTask(UBehaviorTreeComponent&,uint8* Memory)
{ *reinterpret_cast<float*>(Memory)=0.f; return EBTNodeResult::InProgress; }
void UBTTask_HSPursue::TickTask(UBehaviorTreeComponent& Owner,uint8* Memory,float Dt)
{
    auto* AI=Cast<AHSPursuitController>(Owner.GetAIOwner());
    if(!AI || !IsValid(AI->Target)) return;
    auto* Monster=Cast<AHSMonster>(AI->GetPawn());
    const auto* Progress=AI->GetGameInstance()->GetSubsystem<UHSProgression>();
    if(!Monster || Monster->bCinematicActor || Monster->PursuitState==EHSPursuitState::Waiting || Progress->bCinematic || Progress->bCompleted || (!Progress->ActiveRoom.IsNone() && !Progress->bTimerRunning)) { AI->StopMovement(); return; }
    float& Timer=*reinterpret_cast<float*>(Memory); Timer-=Dt;
    if(Timer>0) return; Timer=.3f;
    FAIMoveRequest Request;
    Request.SetGoalActor(AI->Target);
    Request.SetAcceptanceRadius(GetDefault<UHSSettings>()->StopRadius);
    Request.SetReachTestIncludesAgentRadius(false); Request.SetReachTestIncludesGoalRadius(false);
    Request.SetUsePathfinding(true); Request.SetAllowPartialPath(false);
    const auto Result=AI->MoveTo(Request);
    AI->SetBlocked(Result.Code==EPathFollowingRequestResult::Failed);
    if(Result.Code==EPathFollowingRequestResult::Failed) AI->StopMovement();
}
EBTNodeResult::Type UBTTask_HSPursue::AbortTask(UBehaviorTreeComponent& Owner,uint8*)
{ if(Owner.GetAIOwner()) Owner.GetAIOwner()->StopMovement(); return EBTNodeResult::Aborted; }
