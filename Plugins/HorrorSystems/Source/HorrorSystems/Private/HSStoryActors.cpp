#include "HSStoryActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSSettings.h"
#include "HSSceneInteractions.h"
#include "HSAI.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

AHSWoodDoor::AHSWoodDoor()
{
    PawnBlocker=CreateDefaultSubobject<UBoxComponent>(TEXT("WoodDoorCollision"));PawnBlocker->SetupAttachment(Mesh);
    PawnBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);PawnBlocker->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    PawnBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);PawnBlocker->SetCanEverAffectNavigation(true);
    MoveDuration=.55f;
}
void AHSWoodDoor::BeginPlay()
{
    // The base movable-prop animation is replaced by a rotation around the real hinge.
    AActor::BeginPlay();Closed=GetActorTransform();Hinge=Closed.TransformPosition(HingeOffset);
    if(Mesh->GetStaticMesh()) {const auto B=Mesh->GetStaticMesh()->GetBoundingBox();PawnBlocker->SetRelativeLocation(B.GetCenter());PawnBlocker->SetBoxExtent(B.GetExtent().ComponentMax(FVector(1,1,1)));}
    Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    ClosedBlocker=PawnBlocker->GetComponentTransform();
    if(!OpenSound) OpenSound=GetDefault<UHSSettings>()->DoorOpenSound.LoadSynchronous();
    if(!CloseSound) CloseSound=GetDefault<UHSSettings>()->DoorCloseSound.LoadSynchronous();
}
bool AHSWoodDoor::Use(AHSCharacter* Player)
{
    FVector Center,Extent;GetActorBounds(false,Center,Extent);
    if(!Player || FVector::Dist2D(Player->GetActorLocation(),Center)>280.f || bLocked || (bRequiresFinalMessage && !GetGameInstance()->GetSubsystem<UHSProgression>()->bFinalChaseStarted)) return false;
    if(!bOpen && OpenAlpha<=.05f)
    {
        const FVector Normal=Closed.GetRotation().RotateVector(FVector::RightVector);
        const float Side=FVector::DotProduct(Player->GetActorLocation()-Closed.GetLocation(),Normal);
        EffectiveOpenAngle=FMath::Abs(OpenAngle)*(Side>0?-1.f:1.f);
    }
    bOpen=!bOpen;
    if(auto* Sound=bOpen?OpenSound.Get():CloseSound.Get()) UGameplayStatics::PlaySoundAtLocation(this,Sound,Center,.7f);
    return true;
}
void AHSWoodDoor::CloseAndLock()
{
    bLocked=true;
    if(bOpen && CloseSound) UGameplayStatics::PlaySoundAtLocation(this,CloseSound,Closed.GetLocation(),.7f);
    bOpen=false;
    // The player has cleared the door. Keep its closed collision in place while the leaf swings shut.
    PawnBlocker->SetWorldTransform(ClosedBlocker);
}
bool AHSWoodDoor::HasClearedFromSafety(const AHSCharacter* Player,const FVector& SafeCenter) const
{
    if(!Player) return false;
    const FVector Normal=Closed.GetRotation().RotateVector(FVector::RightVector);
    const float SafeSide=FVector::DotProduct(SafeCenter-Closed.GetLocation(),Normal)>=0?1.f:-1.f;
    const float Thickness=Mesh->GetStaticMesh()?Mesh->GetStaticMesh()->GetBoundingBox().GetExtent().Y*FMath::Abs(Closed.GetScale3D().Y):0.f;
    return -SafeSide*FVector::DotProduct(Player->GetActorLocation()-Closed.GetLocation(),Normal)>
        Player->GetCapsuleComponent()->GetScaledCapsuleRadius()+Thickness+3.f;
}
void AHSWoodDoor::Unlock() {bLocked=false;}
void AHSWoodDoor::Tick(float Dt)
{
    AActor::Tick(Dt);
    if(bOpen && Mesh->GetStaticMesh()) PawnBlocker->SetRelativeTransform(FTransform(FQuat::Identity,Mesh->GetStaticMesh()->GetBoundingBox().GetCenter()));
    OpenAlpha=FMath::FInterpConstantTo(OpenAlpha,bOpen?1.f:0.f,Dt,1.f/FMath::Max(.1f,MoveDuration));
    const FQuat Rotation(FVector::UpVector,FMath::DegreesToRadians(EffectiveOpenAngle)*FMath::SmoothStep(0.f,1.f,OpenAlpha));
    SetActorLocationAndRotation(Hinge+Rotation.RotateVector(Closed.GetLocation()-Hinge),Rotation*Closed.GetRotation());
    if(!bOpen) PawnBlocker->SetWorldTransform(ClosedBlocker);
}

AHSFlickerLamp::AHSFlickerLamp()
{
    PrimaryActorTick.bCanEverTick=true;Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("FlickeringBulb"));RootComponent=Light;
    Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetAttenuationRadius(650);Light->SetLightColor(FLinearColor(1.f,.75f,.42f));Light->CastShadows=true;
}
void AHSFlickerLamp::BeginPlay()
{
    Super::BeginPlay();Light->SetIntensity(BaseIntensity);
    if(Fixture) if(auto* Mesh=Fixture->FindComponentByClass<UStaticMeshComponent>()) BulbMaterial=Mesh->CreateDynamicMaterialInstance(0);
    if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0)) bInside=FVector::Dist2D(Player->GetActorLocation(),GetActorLocation())<=EntryRadius;
}
void AHSFlickerLamp::Tick(float Dt)
{
    Super::Tick(Dt);auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(!Player) return;
    const float Distance=FVector::Dist2D(Player->GetActorLocation(),GetActorLocation());
    const bool Inside=bInside?Distance<=EntryRadius+20.f:Distance<=EntryRadius;
    if(Inside && !bInside)
    {
        ++EntryCount;FlickerRemaining=EntryCount==1 && bFirstEntryDoubleFlash?.64f:.32f;
        if(auto* Sound=GetDefault<UHSSettings>()->LampFlickerSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.6f);
    }
    bInside=Inside;FlickerRemaining=FMath::Max(0.f,FlickerRemaining-Dt);
    const bool On=FlickerRemaining<=0 || FMath::Fmod(FlickerRemaining,.32f)<.16f;
    const auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const bool DarkRoom=P->Stage==3 && P->bFinalChaseStarted && P->ActiveRoom==TEXT("RoomB") && !bEmergencyGuidance;
    Light->SetIntensity(On && !DarkRoom?BaseIntensity:0.f);
    if(BulbMaterial) BulbMaterial->SetScalarParameterValue(TEXT("LightStrength"),On && !DarkRoom?1.f:0.f);
}

AHSVisitPainting::AHSVisitPainting() {PrimaryActorTick.bCanEverTick=true;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Painting"));RootComponent=Mesh;}
void AHSVisitPainting::BeginPlay()
{
    Super::BeginPlay();for(int32 I=0;I<Mesh->GetNumMaterials();++I) Artwork.Add(Mesh->GetMaterial(I));Tick(0);
}
void AHSVisitPainting::Tick(float Dt)
{
    Super::Tick(Dt);const auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const bool Blank=P->Stage==1 && P->MapVisits.FindRef(TEXT("Basic_roomA"))<=1;
    for(int32 I=0;I<Artwork.Num();++I) Mesh->SetMaterial(I,Blank && BlankMaterial?BlankMaterial.Get():Artwork[I].Get());
    bBlank=Blank;
}

AHSStoryDirector::AHSStoryDirector() {bPlayFirstEntrySequence=false;bReturnToSafeAfterObjective=true;}
void AHSStoryDirector::BeginPlay() {Super::BeginPlay();ActivateMonster(false);}
void AHSStoryDirector::ActivateMonster(bool Active)
{
    if(!Pursuer) return;Pursuer->bPursuitEnabled=Active;Pursuer->SetActorHiddenInGame(!Active);Pursuer->SetActorEnableCollision(Active);
    Pursuer->RedAuraLight->SetVisibility(Active);
    if(!Active)
    {
        // Hidden actors have no floor collision; freeze movement so gravity cannot drop them below the map.
        Pursuer->GetCharacterMovement()->StopMovementImmediately();Pursuer->GetCharacterMovement()->DisableMovement();
        if(Pursuer->GetController()) Pursuer->GetController()->StopMovement();
    }
    else if(Pursuer->GetCharacterMovement()->MovementMode==MOVE_None) Pursuer->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
void AHSStoryDirector::OnStoryInspectionClosed(UHSItemData* Item)
{
    if(!Item || !Rules) return;auto* P=Progress();
    if(P->StoryStep==1 && Item->ItemId==TEXT("Warning_B") && !P->ReadItems.Contains(Item->ItemId))
    {
        P->ReadItems.Add(Item->ItemId);bNarrativeFinished=true;
        if(auto* Sound=GetDefault<UHSSettings>()->FirstMonsterRoarSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this,Sound,.8f);
        if(WindowSequence) WindowSequence->Play();
        SpawnDelay=WindowSequence?WindowSequence->Duration+.1f:2.5f;
    }
    if(Item->ItemId==TEXT("Testament_C")) P->bCluePanelUnlocked=true;
}
void AHSStoryDirector::Tick(float Dt)
{
    AActor::Tick(Dt);auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* P=Progress();
    if(!P || !Rules || !Player || Find(GetWorld())!=this) return;
    auto* S=PC->GetSession();ReturnBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if(BoundController!=PC) {BoundController=PC;PC->OnInspectionClosed.AddUniqueDynamic(this,&AHSStoryDirector::OnStoryInspectionClosed);}
    if(!bEntered || P->ActiveRoom!=Rules->RoomId)
    {
        bEntered=true;if(P->Stage<MinimumStage) P->SetStage(MinimumStage);P->EnterRoom(Rules->RoomId,Rules->Duration);
        if(bFinalEscapeMode && P->StoryStep<6) P->StoryStep=6;
        bVisitedSafety=IsSafe(Player);
        if(bVisitedSafety) for(TActorIterator<APlayerStart> It(GetWorld());It;++It) if(It->PlayerStartTag==SafeSpawnTag) {S->RecoveryTransform=It->GetActorTransform();S->bHasRecovery=true;break;}
        S->bRecoverySafety=false;bHasLeftSafeArea=false;bSafeTravelReady=false;bSafeAreaSealed=false;
        bNarrativeFinished=P->ReadItems.Contains(TEXT("Warning_B"));
        if(SafeDoor) SafeDoor->Unlock();
        ActivateMonster(P->Stage==2 || (P->Stage==3 && P->bFinalChaseStarted && (Rules->RoomId==TEXT("RoomA") || bMonsterTriggered)));
        if(P->StoryStep==2) PC->Speak(FText::FromString(TEXT("又回到这里了……这次，门后会是哪里？")),6.f);
        if(P->Stage==3 && Rules->RoomId==TEXT("RoomB"))
        {if(auto* Sound=GetDefault<UHSSettings>()->PowerDownSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this,Sound,.7f);PC->Speak(FText::FromString(TEXT("灯灭了……沿着微光走。")),5.f);}
    }
    if(P->bCompleted) {if(EndingTime<=0) BeginEnding();EndingTime+=Dt;if(EndingTime>.15f) S->BeginMenuTravel(false);return;}
    if(S->IsDefeated() || S->bWhiteTransition) return;
    if(SpawnDelay>=0.f && !P->bCinematic)
    {
        // Window elapsed time advances independently; start the chase only after it ends.
        if(P->StoryStep==1 && WindowSequence && WindowSequence->bPlaying) return;
        SpawnDelay=P->StoryStep==1?-1.f:SpawnDelay-Dt;
        if(SpawnDelay<=0.f)
        {
            SpawnDelay=-1;ActivateMonster(true);bMonsterTriggered=true;
            if(P->Stage==3) if(auto* Sound=GetDefault<UHSSettings>()->FinalMonsterRoarSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this,Sound,Pursuer?Pursuer->GetActorLocation():GetActorLocation(),1.f);
            PC->Speak(FText::FromString(P->Stage==3?TEXT("门已经打开……不能回头！"):TEXT("……不妙。")),4.f);
        }
    }
    if(P->bCinematic) return;
    const bool Safe=IsSafe(Player);
    if(Safe) {bVisitedSafety=true;P->bTimerRunning=false;S->RecoveryTransform=FTransform(PC->GetControlRotation(),Player->GetActorLocation());S->bHasRecovery=true;}
    else
    {
        if(bVisitedSafety && !bHasLeftSafeArea)
        {
            const float R=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();const FBox B=SafeArea->Bounds.GetBox();
            if(FVector::DistSquared(Player->GetActorLocation(),B.GetClosestPointTo(Player->GetActorLocation()))>FMath::Square(R+2.f)
                && (!SafeDoor || SafeDoor->HasClearedFromSafety(Player,SafeArea->GetComponentLocation())))
            {bHasLeftSafeArea=true;bSafeAreaSealed=true;if(SafeDoor) SafeDoor->CloseAndLock();P->BeginCountdown();}
        }
        else if(bVisitedSafety && bHasLeftSafeArea && !P->bTimerRunning) {S->bRecoverySafety=false;P->BeginCountdown();}
    }
    const auto* Route=Rules->RouteFor(P->Stage,P->StoryStep);
    bObjectiveAcquired=Route && P->CanExit(*Route);
    if(P->StoryStep==1) bObjectiveAcquired&=bNarrativeFinished && (!WindowSequence || !WindowSequence->bPlaying);
    if(bObjectiveAcquired) {bSafeAreaSealed=false;if(SafeDoor) SafeDoor->Unlock();if(Safe) {ReadyTravelRoute=*Route;bSafeTravelReady=true;}}
    if(P->StoryStep==2 && Route && Safe) {ReadyTravelRoute=*Route;bSafeTravelReady=true;}
    if(P->Stage==2 && P->HasStageClue(2,TEXT("RoomC"),TEXT("Testament_C"))) P->bCluePanelUnlocked=true;
    if(P->Stage==3 && P->HasStageClue(3,TEXT("RoomA"),TEXT("FinalMessage")))
    {
        if(!P->bFinalChaseStarted)
        {P->bFinalChaseStarted=true;SpawnDelay=2.5f;for(const auto& Obstacle:EscapeObstacles) if(Obstacle) Obstacle->StartCollapse();}
        if(SafeDoor) SafeDoor->Unlock();bSafeAreaSealed=false;
        if(Rules->RoomId==TEXT("RoomC") && !bMonsterTriggered && FVector::Dist2D(Player->GetActorLocation(),MonsterTriggerPoint)<=MonsterTriggerRadius)
        {bMonsterTriggered=true;ActivateMonster(true);PC->Speak(FText::FromString(TEXT("它堵住了出口……得把它引开。")),5.f);}
        if(Rules->RoomId==TEXT("RoomB") && !bMonsterTriggered && Player->GetActorLocation().Y<=MonsterTriggerPoint.Y)
        {bMonsterTriggered=true;ActivateMonster(true);}
    }
    if(P->AdvanceClock(Dt)) HandleTimeout();
}
FText AHSStoryDirector::MissionText() const
{
    const auto* P=Progress();
    if(P->Stage==3) return FText::FromString(P->bFinalChaseStarted?TEXT("别回头，穿过 A、B、C，抵达最终出口"):TEXT("找到桌上的最后一张纸条"));
    if(bSafeTravelReady) return FText::FromString(TEXT("穿过安全屋里的白光出口"));
    if(bObjectiveAcquired) return FText::FromString(TEXT("用获得的线索打开木门，赶紧返回安全屋"));
    if(P->StoryStep==1 && P->HasClue(TEXT("Warning_B")) && !bNarrativeFinished) return FText::FromString(TEXT("检视刚拾取的信息"));
    const TCHAR* Tasks[]={TEXT("寻找照片"),TEXT("寻找房间 B 留下的信息"),TEXT("通过安全屋出口前往房间 C"),TEXT("寻找房间 C 的遗言"),TEXT("寻找房间 B 的关键线索"),TEXT("寻找房间 A 的最后一条线索")};
    return FText::FromString(Tasks[FMath::Clamp(P->StoryStep,0,5)]);
}

AHSRoomVariation::AHSRoomVariation()
{
    Writing=CreateDefaultSubobject<UTextRenderComponent>(TEXT("BloodWriting"));RootComponent=Writing;
    Writing->SetText(FText::FromString(TEXT("ROOM 3")));Writing->SetTextRenderColor(FColor(140,12,18));Writing->SetWorldSize(30);
    Writing->SetHorizontalAlignment(EHTA_Center);Writing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AHSRoomVariation::BeginPlay()
{
    Super::BeginPlay();const auto* P=GetGameInstance()->GetSubsystem<UHSProgression>();
    const bool Changed=P->Stage>=MinimumStage;Writing->SetVisibility(Changed);
    if(Changed && Furniture)
    {
        if(auto* Component=Cast<USceneComponent>(Furniture->GetRootComponent())) Component->SetMobility(EComponentMobility::Movable);
        Furniture->AddActorWorldOffset(FurnitureOffset);
    }
}
