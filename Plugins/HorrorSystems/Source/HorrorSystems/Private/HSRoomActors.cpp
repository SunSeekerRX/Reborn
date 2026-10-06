#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSSettings.h"
#include "Sound/SoundBase.h"
#include "HSAI.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AHSRoomDirector::AHSRoomDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    SafeArea=CreateDefaultSubobject<UBoxComponent>(TEXT("SafeArea")); RootComponent=SafeArea;
    SafeArea->SetBoxExtent(FVector(400,420,210)); SafeArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RoomBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBounds")); RoomBounds->SetupAttachment(SafeArea);
    RoomBounds->SetBoxExtent(FVector(750,1400,500)); RoomBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReturnBarrier=CreateDefaultSubobject<UBoxComponent>(TEXT("ReturnBarrier")); ReturnBarrier->SetupAttachment(SafeArea);
    ReturnBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision); ReturnBarrier->SetCollisionResponseToAllChannels(ECR_Ignore);
    ReturnBarrier->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block); ReturnBarrier->SetCanEverAffectNavigation(false);
}
UHSProgression* AHSRoomDirector::Progress() const { return GetGameInstance()->GetSubsystem<UHSProgression>(); }
AHSRoomDirector* AHSRoomDirector::Find(UWorld* W)
{
    if(!W) return nullptr;
    const auto* Player=UGameplayStatics::GetPlayerPawn(W,0);
    AHSRoomDirector* Nearest=nullptr; float Best=MAX_flt;
    for(TActorIterator<AHSRoomDirector> It(W);It;++It)
    {
        if(!It->bUseRoomBounds) return *It;
        if(!Player) continue;
        if(It->RoomBounds->Bounds.GetBox().IsInsideOrOn(Player->GetActorLocation())) return *It;
        const float D=FVector::DistSquared(It->RoomBounds->GetComponentLocation(),Player->GetActorLocation());
        if(D<Best) {Best=D; Nearest=*It;}
    }
    return Nearest;
}
void AHSRoomDirector::PrepareEntry()
{ bInitialized=false; bSafeAreaSealed=false; bResetting=false; ReturnBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
bool AHSRoomDirector::IsSafe(const AHSCharacter* C) const
{ return C && SafeArea->Bounds.GetBox().IsInsideOrOn(C->GetActorLocation()); }
void AHSRoomDirector::Tick(float Dt)
{
    if(GetGameInstance()->GetSubsystem<UHSWorldState>()->IsDefeated()) return;
    Super::Tick(Dt);
    if(Find(GetWorld())!=this) return;
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
    auto* P=Progress(); if(!Rules || !PC || !Player || !P) return;
    if(!bInitialized || P->ActiveRoom!=Rules->RoomId)
    {
        bInitialized=true; P->EnterRoom(Rules->RoomId,Rules->Duration);
        ReturnBarrier->SetBoxExtent(SafeArea->GetUnscaledBoxExtent());
        if(bPlayFirstEntrySequence && WindowSequence && !P->SeenWindows.Contains(Rules->RoomId))
        { P->SeenWindows.Add(Rules->RoomId); WindowSequence->Play(); }
    }
    if(P->bCompleted)
    {
        if(EndingTime<=0) BeginEnding();
        EndingTime+=Dt; return;
    }
    if(bResetting || P->bCinematic) return;
    // Wait until the capsule clears the safe area before turning on its solid volume.
    const FBox SafeBox=SafeArea->Bounds.GetBox();
    const float Radius=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
    if(!bSafeAreaSealed && FVector::DistSquared(Player->GetActorLocation(),SafeBox.GetClosestPointTo(Player->GetActorLocation()))>FMath::Square(Radius+2.f))
    { bSafeAreaSealed=true; ReturnBarrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); P->BeginCountdown(); }
    if(P->AdvanceClock(Dt)) HandleTimeout();
}
void AHSRoomDirector::FinishIntro() { if(WindowSequence) WindowSequence->Finish(); }
void AHSRoomDirector::HandleTimeout()
{
    if(bResetting || Progress()->bCompleted) return;
    bResetting=true; FinishIntro();
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if(PC) { PC->CloseInspection(); PC->SetGameplayLocked(true); }
    Progress()->RollbackRoom();
    auto* Inventory=GetGameInstance()->GetSubsystem<UHSWorldState>();
    Inventory->PendingSpawnTag=SafeSpawnTag; Inventory->bTravelPending=true;
    UGameplayStatics::OpenLevel(this,FName(*GetWorld()->GetOutermost()->GetName()));
}
bool AHSRoomDirector::ChangeStage(int32 NewStage)
{
    if(Progress()->bCinematic || bResetting || !Progress()->SetStage(NewStage)) return false;
    HandleTimeout(); return true;
}
void AHSRoomDirector::BeginEnding()
{
    EndingTime=.001f;
    if(auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
    { PC->CloseInspection(); PC->SetGameplayLocked(true); PC->bShowMouseCursor=true; PC->SetInputMode(FInputModeGameAndUI()); }
    for(TActorIterator<AHSMonster> It(GetWorld());It;++It) { It->GetCharacterMovement()->StopMovementImmediately(); if(It->GetController()) It->GetController()->StopMovement(); }
}
AHSWindowSequence::AHSWindowSequence()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("WindowRoot"));
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("WindowCamera")); Camera->SetupAttachment(RootComponent);
    Camera->FieldOfView=65;
}
void AHSWindowSequence::Play()
{
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0)); if(!PC || bPlaying) return;
    PC->GetPlayerViewPoint(StartView,StartRotation);
    if(auto* Player=Cast<AHSCharacter>(PC->GetPawn()))
    { StartView=Player->Camera->GetComponentLocation();StartRotation=PC->GetControlRotation(); }
    CameraPath.Reset();CameraPathDistances.Reset();CurrentPathDistance=0.f;
    CameraPath.Add(StartView);CameraPathDistances.Add(0.f);
    for(const FVector& Point:CameraWaypoints) CameraPath.Add(GetActorTransform().TransformPosition(Point));
    CameraPath.Add(GetActorTransform().TransformPosition(CameraOffset));
    for(int32 I=1;I<CameraPath.Num();++I) CameraPathDistances.Add(CameraPathDistances.Last()+FVector::Distance(CameraPath[I-1],CameraPath[I]));
    bPlaying=true; Elapsed=0;
    GetGameInstance()->GetSubsystem<UHSProgression>()->bCinematic=true;
    if(auto* Sound=GetDefault<UHSSettings>()->MonsterEntranceSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this,Sound,.5f);
    Camera->SetWorldLocationAndRotation(StartView,StartRotation); PC->SetGameplayLocked(true); PC->SetViewTarget(this);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Performer=GetWorld()->SpawnActor<AHSMonster>(AHSMonster::StaticClass(),GetActorTransform().TransformPosition(MonsterStart),GetActorRotation(),Spawn);
    if(Performer)
    {
        Performer->bCinematicActor=true;
        Performer->GetCharacterMovement()->bOrientRotationToMovement=false;
        Performer->GetCharacterMovement()->GravityScale=0;
        Performer->GetCharacterMovement()->DisableMovement(); Performer->GetCharacterMovement()->SetComponentTickEnabled(false);
        Performer->AddTickPrerequisiteActor(this); Performer->GetMesh()->AddTickPrerequisiteActor(this);
        Performer->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(auto* AI=Cast<AAIController>(Performer->GetController())) { AI->StopMovement(); if(AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Window performance")); }
    }
}
void AHSWindowSequence::MoveCameraAlongPath(float Alpha,const FQuat& Rotation)
{
    if(CameraPath.Num()<2) return;
    const float Distance=FMath::Clamp(Alpha,0.f,1.f)*CameraPathDistances.Last();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HSWindowCamera),true,this);
    if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0)) Params.AddIgnoredActor(Player);
    if(Performer) Params.AddIgnoredActor(Performer);
    auto MoveTo=[&](const FVector& Position)
    {
        FHitResult Hit;
        if(GetWorld()->SweepSingleByChannel(Hit,Camera->GetComponentLocation(),Position,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(FMath::Max(12.f,CameraCollisionRadius)),Params))
        {
            if(!Hit.bStartPenetrating) Camera->SetWorldLocation(Hit.Location+Hit.Normal*2.f);
            return false;
        }
        Camera->SetWorldLocation(Position);return true;
    };
    // Visit crossed waypoints even after a long frame instead of cutting their corners.
    if(Distance>=CurrentPathDistance)
    {
        for(int32 I=1;I<CameraPath.Num();++I)
            if(CameraPathDistances[I]>CurrentPathDistance && CameraPathDistances[I]<Distance && !MoveTo(CameraPath[I])) return;
    }
    else
    {
        for(int32 I=CameraPath.Num()-2;I>=0;--I)
            if(CameraPathDistances[I]<CurrentPathDistance && CameraPathDistances[I]>Distance && !MoveTo(CameraPath[I])) return;
    }
    int32 Segment=1;
    while(Segment<CameraPath.Num()-1 && CameraPathDistances[Segment]<Distance) ++Segment;
    const float Fraction=(Distance-CameraPathDistances[Segment-1])/FMath::Max(.001f,CameraPathDistances[Segment]-CameraPathDistances[Segment-1]);
    if(MoveTo(FMath::Lerp(CameraPath[Segment-1],CameraPath[Segment],Fraction))) CurrentPathDistance=Distance;
    Camera->SetWorldRotation(Rotation);
}
void AHSWindowSequence::Tick(float Dt)
{
    Super::Tick(Dt); if(!bPlaying) return;
    Elapsed+=Dt;
    const float T=Elapsed/FMath::Max(1.f,Duration);
    const FVector Destination=GetActorTransform().TransformPosition(CameraOffset);
    const FRotator Rotation=GetActorRotation()+WindowViewRotation;
    if(T<.22f)
    { const float A=FMath::SmoothStep(0.f,1.f,T/.22f); MoveCameraAlongPath(A,FQuat::Slerp(StartRotation.Quaternion(),Rotation.Quaternion(),A)); }
    else if(T<.88f) MoveCameraAlongPath(1.f,Rotation.Quaternion());
    else
    {
        auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0));
        auto* C=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        if(C) { const float A=FMath::SmoothStep(0.f,1.f,(T-.88f)/.12f); MoveCameraAlongPath(1.f-A,FQuat::Slerp(Rotation.Quaternion(),PC->GetControlRotation().Quaternion(),A)); }
    }
    if(Performer)
    {
        const FVector Old=Performer->GetActorLocation();
        FVector Local;
        if(T<.25f) Local=MonsterStart;
        else if(T<.58f) Local=FMath::Lerp(MonsterStart,MonsterLookPoint,(T-.25f)/.33f);
        else if(T<.70f) Local=MonsterLookPoint;
        else Local=FMath::Lerp(MonsterLookPoint,MonsterEnd,FMath::Clamp((T-.70f)/.18f,0.f,1.f));
        const FVector World=GetActorTransform().TransformPosition(Local); Performer->SetActorLocation(World);
        Performer->GetCharacterMovement()->Velocity=(World-Old)/FMath::Max(.001f,Dt);
        const FRotator Look=T>=.58f && T<.70f ? (Destination-World).Rotation() : GetActorRotation();
        Performer->SetActorRotation(FMath::RInterpTo(Performer->GetActorRotation(),FRotator(0,Look.Yaw,0),Dt,8.f));
    }
    if(T>=1) Finish();
}
void AHSWindowSequence::Finish()
{
    if(!bPlaying) return;
    bPlaying=false; GetGameInstance()->GetSubsystem<UHSProgression>()->bCinematic=false;
    if(Performer) Performer->Destroy(); Performer=nullptr;
    if(auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0))) { PC->SetViewTarget(PC->GetPawn()); PC->SetGameplayLocked(false); }
}
AHSMovableProp::AHSMovableProp()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovableMesh")); RootComponent=Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void AHSMovableProp::BeginPlay() { Super::BeginPlay(); ClosedTransform=GetActorTransform(); }
void AHSMovableProp::Tick(float Dt)
{
    Super::Tick(Dt); Alpha=FMath::FInterpConstantTo(Alpha,bOpen?1.f:0.f,Dt,1.f/FMath::Max(.1f,MoveDuration));
    const float A=FMath::SmoothStep(0.f,1.f,Alpha);
    const FVector Target=ClosedTransform.TransformPosition(OpenOffset);
    SetActorLocationAndRotation(FMath::Lerp(ClosedTransform.GetLocation(),Target,A),FQuat::Slerp(ClosedTransform.GetRotation(),ClosedTransform.GetRotation()*OpenRotation.Quaternion(),A));
}
bool AHSMovableProp::Interact(AHSCharacter* C)
{
    if(!C || FVector::Dist(C->GetActorLocation(),GetActorLocation())>280.f) return false;
    bOpen=!bOpen;
    if(bOpen) GetGameInstance()->GetSubsystem<UHSProgression>()->CollectClue(ClueOnOpen);
    if(InteractionSound) UGameplayStatics::PlaySoundAtLocation(this,InteractionSound,GetActorLocation(),.5f);
    return true;
}
