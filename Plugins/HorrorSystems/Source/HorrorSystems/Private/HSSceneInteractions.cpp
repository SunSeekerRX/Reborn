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
    if(LastStage!=P->Stage)
    {
        LastStage=P->Stage;Music->Stop();
        Music->SetSound(P->Stage>=2?GetDefault<UHSSettings>()->Stage2Music.LoadSynchronous():nullptr);
        if(Music->Sound) Music->FadeIn(1.f,MusicVolume);
    }
    float Distance=MAX_flt;
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(Player && P->bTimerRunning && !P->bCompleted && !GetGameInstance()->GetSubsystem<UHSWorldState>()->IsDefeated())
        for(TActorIterator<AHSMonster> It(GetWorld());It;++It) if(!It->bCinematicActor && (It->HomeRoom.IsNone() || It->HomeRoom==P->ActiveRoom)) Distance=FMath::Min(Distance,float(FVector::Dist2D(Player->GetActorLocation(),It->GetActorLocation())));
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
    if(bFalling || bCollapsed) return;
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
    if(T>=1.f) {bFalling=false;bCollapsed=true;Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);}
}
AHSInspectTrigger::AHSInspectTrigger()
{
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InspectableMesh"));RootComponent=Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->bDisallowNanite=true;
}
bool AHSInspectTrigger::Interact(AHSCharacter* Player)
{
    auto* PC=Player?Cast<AHSPlayerController>(Player->GetController()):nullptr;
    if(!PC || !ItemData || PC->IsInspecting() || PC->IsGameplayLocked() || FVector::Dist(Player->GetActorLocation(),GetActorLocation())>280.f) return false;
    PC->InspectItem(ItemData);
    if(!PC->IsInspecting()) return false;
    InspectingController=PC; bArmed=true;
    PC->OnInspectionClosed.AddUniqueDynamic(this,&AHSInspectTrigger::OnInspectionClosed);
    return true;
}
void AHSInspectTrigger::OnInspectionClosed(UHSItemData* Item)
{
    if(!bArmed || Item!=ItemData) return;
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
    From=GetActorLocation(); To=Other->GetActorLocation();
    Other->From=To;Other->To=From;Elapsed=Other->Elapsed=0.f;bSwapping=Other->bSwapping=true;
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
