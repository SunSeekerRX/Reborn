#include "HSWorldActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSSettings.h"
#include "HSProgression.h"
#include "HSRoomActors.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AHSPickup::AHSPickup()
{
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); RootComponent=Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Mesh->SetStaticMesh(Shape.Object); Mesh->SetRelativeScale3D(FVector(.28f,.28f,.12f));
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(Mesh);
    Label->SetRelativeLocation(FVector(0,0,180)); Label->SetWorldSize(55);
    Label->SetHorizontalAlignment(EHTA_Center);
}
FString AHSPickup::GetPersistentKey() const
{
    const FString Level=UGameplayStatics::GetCurrentLevelName(this,true);
    return Level+TEXT(":")+(PickupId.IsNone()?GetName():PickupId.ToString());
}
void AHSPickup::BeginPlay()
{
    Super::BeginPlay();
    Label->SetVisibility(false);
    const auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    if(Progress->Stage<MinimumStage || Progress->Stage>MaximumStage) { Destroy(); return; }
    if(auto* S=GetGameInstance()->GetSubsystem<UHSWorldState>(); S && S->CollectedPickups.Contains(GetPersistentKey())) { Destroy(); return; }
    Label->SetText(FText::FromString(ItemData ? ItemData->ItemId.ToString().Replace(TEXT("DA_"),TEXT("")) : TEXT("Missing ItemData")));
    if(!ItemData) UE_LOG(LogTemp,Warning,TEXT("HorrorSystems: %s has no ItemData"),*GetName());
}
void AHSPickup::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bRotateForDemo) AddActorLocalRotation(FRotator(0,Dt*22,0));
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
        Label->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}
bool AHSPickup::TryPickup(AHSCharacter* Character)
{
    if(!Character || bClaimed || !ItemData) return false;
    auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    if(Progress->bCinematic || Progress->bCompleted) return false;
    if(FVector::Dist(Character->GetActorLocation(),GetActorLocation())>GetDefault<UHSSettings>()->PickupDistance) return false;
    auto* PC=Cast<AHSPlayerController>(Character->GetController());
    auto* Session=PC ? PC->GetSession() : nullptr;
    if(!Session || !Session->TryAddItem(ItemData,GetPersistentKey()))
    {
        if(PC) PC->Notify(FText::FromString(TEXT("快捷栏已满（10/10），无法拾取")));
        return false;
    }
    bClaimed=true; Progress->CollectClue(ClueId);
    if(ItemData->PickupSound) UGameplayStatics::PlaySound2D(this,ItemData->PickupSound,.6f);
    PC->Notify(FText::Format(FText::FromString(TEXT("已拾取：{0} · 按 R 检视")),ItemData->DisplayName));
    Character->StartPickupAnimation(ItemData,GetActorLocation());
    Character->OnItemPickedUp(ItemData);
    // Character waits until the first-person pickup animation finishes before inspection.
    Destroy(); return true;
}

AHSPortal::AHSPortal()
{
    PrimaryActorTick.bCanEverTick=true;
    Trigger=CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger")); RootComponent=Trigger;
    Trigger->SetBoxExtent(FVector(90,120,150)); Trigger->SetCollisionProfileName(TEXT("Trigger"));
    Trigger->OnComponentBeginOverlap.AddDynamic(this,&AHSPortal::OnOverlap);
    Marker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(Trigger);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Marker->SetStaticMesh(Shape.Object); Marker->SetRelativeScale3D(FVector(.12f,2.4f,3.f));
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(Trigger);
    Label->SetRelativeLocation(FVector(0,0,185)); Label->SetWorldSize(30); Label->SetHorizontalAlignment(EHTA_Center);
}
void AHSPortal::BeginPlay()
{
    Super::BeginPlay(); Label->SetVisibility(false);
    if(!TravelSound) TravelSound=LoadObject<USoundBase>(nullptr,TEXT("/HorrorSystems/Audio/S_Travel.S_Travel"));
}
bool AHSPortal::IsLocked() const
{
    if(!bUseStageRoute) return false;
    const auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const auto* Route=RoomRules?RoomRules->RouteFor(P->Stage):nullptr;
    return !Route || !P->CanExit(*Route);
}
void AHSPortal::Tick(float Dt)
{
    Super::Tick(Dt); if(!bUseStageRoute || bUsed) return;
    Marker->SetCollisionResponseToAllChannels(ECR_Ignore); Marker->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Marker->SetCollisionEnabled(IsLocked()?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    if(!IsLocked()) if(auto* C=Cast<AHSCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); C && Trigger->IsOverlappingActor(C)) Travel(C);
}
void AHSPortal::OnOverlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{ if(auto* Character=Cast<AHSCharacter>(Other)) Travel(Character); }
bool AHSPortal::Travel(AHSCharacter* Character)
{
    if(!Character || bUsed || GetWorld()->GetTimeSeconds()<1.f || IsLocked()) return false;
    auto* PC=Cast<AHSPlayerController>(Character->GetController());
    if(!PC || PC->IsInspecting()) return false;
    if(PC->IsGameplayLocked()) return false;
    auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    const auto* Route=bUseStageRoute && RoomRules?RoomRules->RouteFor(Progress->Stage):nullptr;
    if(Route && Route->bFinishAtFinalStage && Progress->Stage==3)
    { Progress->CommitExit(*Route); bUsed=true; if(auto* Director=AHSRoomDirector::Find(GetWorld())) Director->BeginEnding(); return true; }
    const TSoftObjectPtr<UWorld> NextLevel=Route?Route->Destination:Destination;
    // Fail before modifying session state if the configured destination is missing.
    UWorld* Level=NextLevel.LoadSynchronous();
    if(!Level) { PC->Notify(FText::FromString(TEXT("传送失败：请配置有效目标关卡"))); return false; }
    auto* S=PC->GetSession(); if(!S) return false;
    if(S->bTravelPending) return false;
    if(Route) Progress->CommitExit(*Route);
    bUsed=true; S->bTravelPending=true; S->PendingSpawnTag=bUseStageRoute?FName(TEXT("Safe")):DestinationSpawnTag;
    if(TravelSound) UGameplayStatics::SpawnSound2D(this,TravelSound,.6f,1.f,0.f,nullptr,true,true);
    UGameplayStatics::OpenLevelBySoftObjectPtr(this,NextLevel);
    return true;
}

AHSAmbientZone::AHSAmbientZone()
{
    Audio=CreateDefaultSubobject<UAudioComponent>(TEXT("Audio")); RootComponent=Audio;
    Audio->bAutoActivate=false; Audio->bAllowSpatialization=true; Audio->bOverrideAttenuation=true;
}
void AHSAmbientZone::BeginPlay()
{
    Super::BeginPlay();
    if(!Sound) Sound=LoadObject<USoundBase>(nullptr,TEXT("/HorrorSystems/Audio/S_Ambience.S_Ambience"));
    Audio->SetSound(Sound); Audio->SetVolumeMultiplier(Volume);
    Audio->AttenuationOverrides.bAttenuate=true;
    Audio->AttenuationOverrides.AttenuationShape=EAttenuationShape::Sphere;
    Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(AudibleRadius*.35f,0,0);
    Audio->AttenuationOverrides.FalloffDistance=AudibleRadius*.65f;
    Audio->Play();
}

AHSVisionRig::AHSVisionRig()
{
    Fog=CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog")); RootComponent=Fog;
    Fog->SetFogDensity(.08f); Fog->SetFogHeightFalloff(.01f);
    Fog->SetFogInscatteringColor(FLinearColor(.005f,.008f,.014f));
    Fog->SetFogMaxOpacity(1.f); Fog->SetStartDistance(350.f);
    PostProcess=CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess")); PostProcess->SetupAttachment(Fog);
    PostProcess->bUnbound=true;
    PostProcess->Settings.bOverride_VignetteIntensity=true; PostProcess->Settings.VignetteIntensity=.65f;
    PostProcess->Settings.bOverride_AutoExposureMinBrightness=true; PostProcess->Settings.AutoExposureMinBrightness=1.f;
    PostProcess->Settings.bOverride_AutoExposureMaxBrightness=true; PostProcess->Settings.AutoExposureMaxBrightness=1.f;
    PostProcess->Settings.bOverride_MotionBlurAmount=true; PostProcess->Settings.MotionBlurAmount=0;
    PostProcess->Settings.bOverride_AutoExposureBias=true; PostProcess->Settings.AutoExposureBias=-1.f;
}
void AHSVisionRig::BeginPlay()
{
    Super::BeginPlay();
    if(bUseProjectSettings)
    {
        const auto* S=GetDefault<UHSSettings>();
        Fog->SetFogDensity(S->FogDensity); Fog->SetStartDistance(S->ClearRadius*.6f);
    }
    // Depth-based fade also hides emissive objects and distant characters, without
    // removing large floor collision or NavMesh geometry from the world.
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/HorrorSystems/Materials/M_DistanceFade.M_DistanceFade")))
    {
        auto* Dynamic=UMaterialInstanceDynamic::Create(Material,this);
        const auto* S=GetDefault<UHSSettings>();
        Dynamic->SetScalarParameterValue(TEXT("ClearDistance"),S->ClearRadius);
        Dynamic->SetScalarParameterValue(TEXT("HiddenDistance"),FMath::Max(S->HiddenRadius,S->ClearRadius+100.f));
        PostProcess->AddOrUpdateBlendable(Dynamic,1.f);
    }
}
