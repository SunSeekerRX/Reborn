#include "HSSceneInteractions.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSItemData.h"
#include "HSSettings.h"
#include "Sound/SoundBase.h"
#include "HSProgression.h"
#include "HSWorldState.h"
#include "HSAI.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BoxComponent.h"
#include "HSRoomActors.h"

AHSRecoveryCheckpoint::AHSRecoveryCheckpoint()
{
    PrimaryActorTick.bCanEverTick=true;SafeArea=CreateDefaultSubobject<UBoxComponent>(TEXT("CheckpointArea"));RootComponent=SafeArea;
    SafeArea->SetBoxExtent(FVector(140,140,200));SafeArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    SafeArea->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);SafeArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SafeArea->SetCanEverAffectNavigation(false);
}
void AHSRecoveryCheckpoint::Tick(float Dt)
{
    Super::Tick(Dt);auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const bool Unlocked=P->Stage==3 && (P->bFinalChaseStarted || P->HasStageClue(3,TEXT("RoomA"),TEXT("Key_3")));
    SafeArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Player=Cast<AHSCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Unlocked || !Player || !SafeArea->Bounds.GetBox().IsInsideOrOn(Player->GetActorLocation())) return;
    auto* S=GetGameInstance()->GetSubsystem<UHSWorldState>();S->RecoveryTransform=SpawnTransform;S->bHasRecovery=true;S->bRecoverySafety=true;P->bTimerRunning=false;
}

AHSSceneAudio::AHSSceneAudio()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("AudioRoot"));
    Ambient=CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientBackground"));Ambient->SetupAttachment(RootComponent);
    Music=CreateDefaultSubobject<UAudioComponent>(TEXT("StageMusic"));Music->SetupAttachment(RootComponent);
    Pressure=CreateDefaultSubobject<UAudioComponent>(TEXT("PlayerPressure"));Pressure->SetupAttachment(RootComponent);
    for(auto* Audio:{Ambient.Get(),Music.Get(),Pressure.Get()}) {Audio->bAutoActivate=false;Audio->bAllowSpatialization=false;}
}
void AHSSceneAudio::BeginPlay()
{
    Super::BeginPlay();const auto* Settings=GetDefault<UHSSettings>();
    Ambient->SetSound(Settings->AmbientBackgroundSound.LoadSynchronous());Ambient->SetVolumeMultiplier(AmbientVolume);
    if(Ambient->Sound) Ambient->Play();
    Music->SetVolumeMultiplier(MusicVolume);Pressure->SetSound(Settings->PlayerPressureSound.LoadSynchronous());Pressure->SetVolumeMultiplier(0);
}
void AHSSceneAudio::Tick(float Dt)
{
    Super::Tick(Dt);const auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const auto* Session=GetGameInstance()->GetSubsystem<UHSWorldState>();
    if(P->bCompleted) {if(LastStage!=-2) {Ambient->FadeOut(.35f,0);Music->FadeOut(.35f,0);Pressure->FadeOut(.35f,0);LastStage=-2;}return;}
    const auto* Character=Cast<AHSCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    const bool Safe=Session->IsInSafety(Character);
    const bool Chase=!Safe && !P->bCinematic && P->bTimerRunning && ((P->Stage==3 && P->bFinalChaseStarted) || (P->Stage==1 && P->ReadItems.Contains(TEXT("Warning_B")) && P->ActiveRoom==TEXT("RoomB")));
    const int32 MusicState=Chase?3:Safe?0:P->Stage==2?2:1;
    if(LastStage!=MusicState)
    {
        LastStage=MusicState;Music->Stop();
        Music->SetSound(Chase?GetDefault<UHSSettings>()->ChaseMusic.LoadSynchronous():MusicState==2?GetDefault<UHSSettings>()->Stage2Music.LoadSynchronous():nullptr);
        if(Music->Sound) Music->FadeIn(1.f,MusicVolume);
        auto* Background=Safe?GetDefault<UHSSettings>()->SafeRoomSound.LoadSynchronous():GetDefault<UHSSettings>()->AmbientBackgroundSound.LoadSynchronous();
        if(Ambient->Sound!=Background) {Ambient->Stop();Ambient->SetSound(Background);if(Background) Ambient->FadeIn(.4f,AmbientVolume);}
    }
    float Distance=MAX_flt;
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(Player && P->bTimerRunning && !P->bCompleted && !GetGameInstance()->GetSubsystem<UHSWorldState>()->IsDefeated())
        for(TActorIterator<AHSMonster> It(GetWorld());It;++It) if(It->bPursuitEnabled && !It->bCinematicActor && (It->HomeRoom.IsNone() || It->HomeRoom==P->ActiveRoom)) Distance=FMath::Min(Distance,float(FVector::Dist2D(Player->GetActorLocation(),It->GetActorLocation())));
    const float Volume=PressureVolume*FMath::Clamp((700.f-Distance)/400.f,0.f,1.f);
    Pressure->SetVolumeMultiplier(Volume);
    if(Volume>.001f && Pressure->Sound && !Pressure->IsPlaying()) Pressure->Play();
    if(Volume<=.001f && Pressure->IsPlaying()) Pressure->Stop();
    if(P->bCompleted || GetGameInstance()->GetSubsystem<UHSWorldState>()->IsDefeated()) {Ambient->Stop();Music->Stop();Pressure->Stop();}
}

AHSCollapsingObstacle::AHSCollapsingObstacle()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObstacleMesh")); RootComponent=Mesh;
    Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCanEverAffectNavigation(true);
}
void AHSCollapsingObstacle::StartCollapse()
{
    if(bFalling || bCollapsed || GetGameInstance()->GetSubsystem<UHSProgression>()->Stage<MinimumCollapseStage) return;
    Start=GetActorTransform(); Pivot=Start.TransformPosition(FallPivotOffset); Elapsed=0.f; bFalling=true;
    auto* Sound=CollapseSound?CollapseSound.Get():GetDefault<UHSSettings>()->CollapseSound.LoadSynchronous();
    if(Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation());
}
void AHSCollapsingObstacle::Tick(float Dt)
{
    Super::Tick(Dt); if(!bFalling) return;
    Elapsed+=Dt;
    const float T=FMath::Clamp(Elapsed/FMath::Max(.1f,FallDuration),0.f,1.f);
    const FQuat Rotation(FallAxis.GetSafeNormal(),FMath::DegreesToRadians(FallAngle)*T*T);
    SetActorLocationAndRotation(Pivot+Rotation.RotateVector(Start.GetLocation()-Pivot),Rotation*Start.GetRotation());
    if(T>=1.f) {bFalling=false;bCollapsed=true;Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);if(auto* Sound=GetDefault<UHSSettings>()->CabinetImpactSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.5f);}
}
AHSInspectTrigger::AHSInspectTrigger()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InspectableMesh"));RootComponent=Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->bDisallowNanite=true;
}
FString AHSInspectTrigger::GetPickupKey() const
{ return UGameplayStatics::GetCurrentLevelName(this,true)+TEXT(":Record:")+GetName(); }
void AHSInspectTrigger::BeginPlay()
{
    Super::BeginPlay();
    if(!bCollectToHotbar) return;
    auto* S=GetGameInstance()->GetSubsystem<UHSWorldState>();
    for(const auto& Slot:S->Slots) if(Slot.WorldPickupKey==GetPickupKey() && Slot.Item)
    { CollectedItem=Slot.Item;bCollected=true;bArmed=true;break; }
    if(bCollected || S->CollectedPickups.Contains(GetPickupKey()))
    {bCollected=true;SetActorHiddenInGame(true);SetActorEnableCollision(false);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
    Tick(0);
}
void AHSInspectTrigger::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bCollected && Obstacle)
    {
        const bool Available=GetGameInstance()->GetSubsystem<UHSProgression>()->Stage>=Obstacle->MinimumCollapseStage;
        if(IsHidden()==Available)
        {SetActorHiddenInGame(!Available);SetActorEnableCollision(Available);Mesh->SetCollisionEnabled(Available?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);}
    }
    if(bArmed && !InspectingController.IsValid())
        if(auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
        { InspectingController=PC;PC->OnInspectionClosed.AddUniqueDynamic(this,&AHSInspectTrigger::OnInspectionClosed); }
}
bool AHSInspectTrigger::Interact(AHSCharacter* Player)
{
    auto* PC=Player?Cast<AHSPlayerController>(Player->GetController()):nullptr;
    if(!PC || !ItemData || bCollected || PC->IsInspecting() || PC->IsGameplayLocked() || FVector::Dist(Player->GetActorLocation(),GetActorLocation())>280.f) return false;
    if(Obstacle && GetGameInstance()->GetSubsystem<UHSProgression>()->Stage<Obstacle->MinimumCollapseStage) return false;
    if(bCollectToHotbar)
    {
        // Each note owns its inspection identity even when all use the same asset.
        // Closing one collected note must only trigger its corresponding obstacle.
        auto* Instance=DuplicateObject<UHSItemData>(ItemData,PC->GetSession());
        Instance->SetFlags(RF_Transient);Instance->ClearFlags(RF_Public|RF_Standalone);
        if(!PC->GetSession()->TryAddItem(Instance,GetPickupKey())) return false;
        CollectedItem=Instance;bCollected=true;
        Player->StartPickupAnimation(Instance,GetActorLocation());Player->OnItemPickedUp(Instance);
        if(Instance->PickupSound) UGameplayStatics::PlaySound2D(this,Instance->PickupSound,.6f);
        SetActorHiddenInGame(true);SetActorEnableCollision(false);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    else {PC->InspectItem(ItemData);if(!PC->IsInspecting()) return false;}
    InspectingController=PC; bArmed=true;
    PC->OnInspectionClosed.AddUniqueDynamic(this,&AHSInspectTrigger::OnInspectionClosed);
    return true;
}
void AHSInspectTrigger::OnInspectionClosed(UHSItemData* Item)
{
    if(!bArmed || Item!=(CollectedItem?CollectedItem.Get():ItemData.Get())) return;
    bArmed=false;
    if(InspectingController.IsValid()) InspectingController->OnInspectionClosed.RemoveDynamic(this,&AHSInspectTrigger::OnInspectionClosed);
    if(Obstacle) Obstacle->StartCollapse();
}
void AHSInspectTrigger::EndPlay(const EEndPlayReason::Type Reason)
{
    if(InspectingController.IsValid()) InspectingController->OnInspectionClosed.RemoveDynamic(this,&AHSInspectTrigger::OnInspectionClosed);
    Super::EndPlay(Reason);
}
AHSSwapPainting::AHSSwapPainting()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaintingMesh"));RootComponent=Mesh;
    Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->bDisallowNanite=true;
    Mesh->SetCanEverAffectNavigation(false);
}
bool AHSSwapPainting::SwapWith(AHSSwapPainting* Other)
{
    if(!IsValid(Other) || Other==this || SwapGroup!=Other->SwapGroup || bSwapping || Other->bSwapping) return false;
    // Imported frames may have different pivots: exchange visible centers, preserving every component.
    const FVector Delta=Other->Mesh->Bounds.Origin-Mesh->Bounds.Origin;
    From=GetActorLocation();To=From+Delta;
    Other->From=Other->GetActorLocation();Other->To=Other->From-Delta;
    Elapsed=Other->Elapsed=0.f;bSwapping=Other->bSwapping=true;
    Other->SwapDuration=SwapDuration;
    auto* Sound=SwapSound?SwapSound.Get():GetDefault<UHSSettings>()->PaintingSwapSound.LoadSynchronous();
    if(Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,(From+To)*.5f);
    return true;
}
void AHSSwapPainting::Tick(float Dt)
{
    Super::Tick(Dt);if(!bSwapping) return;
    Elapsed+=Dt;const float T=FMath::Clamp(Elapsed/FMath::Max(.1f,SwapDuration),0.f,1.f);
    // Opposite shallow arcs prevent the two paintings occupying the same midpoint.
    const FVector Lift(FMath::Sin(T*PI)*(From.Y<To.Y?85.f:25.f),0,FMath::Sin(T*PI)*(From.Y<To.Y?100.f:-100.f));
    SetActorLocation(FMath::Lerp(From,To,FMath::SmoothStep(0.f,1.f,T))+Lift);
    if(T>=1.f) {SetActorLocation(To);bSwapping=false;}
}
