#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "HSProgression.h"
#include "HSWorldState.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSAI.h"
#include "HSSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraComponent.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSProgressionPresence,"HorrorSystems.Progression.IntegrationAvailable",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ProductFilter)
bool FHSProgressionPresence::RunTest(const FString&)
{
    TestNotNull(TEXT("Cross-level stage manager exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSProgression")));
    TestNotNull(TEXT("Room safety and countdown director exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSRoomDirector")));
    TestNotNull(TEXT("Window cinematic exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSWindowSequence")));
    TestNotNull(TEXT("Movable interactive prop exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSMovableProp")));
    return true;
}
namespace
{
UWorld* ProgressionWorld()
{ for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) return C.World(); return nullptr; }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSProgressionRulesTest,"HorrorSystems.Progression.TimerRollbackAndStages",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSProgressionRulesTest::RunTest(const FString&)
{
    auto* W=ProgressionWorld(); if(!W) { AddError(TEXT("No game world")); return false; }
    auto* Inventory=W->GetGameInstance()->GetSubsystem<UHSWorldState>();
    auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
    Inventory->ResetSession();
    auto* Item=LoadObject<UHSItemData>(nullptr,TEXT("/HorrorSystems/Items/DA_Key.DA_Key"));
    Inventory->TryAddItem(Item,TEXT("PriorRoom:Keep"));
    P->EnterRoom(TEXT("RoomA"),60);
    TestFalse(TEXT("Safe area does not consume time"),P->AdvanceClock(20)); TestEqual(TEXT("Safe 60 seconds"),P->GetRemaining(),60.f);
    P->BeginCountdown(); P->AdvanceClock(5); TestEqual(TEXT("Room A runs"),P->GetRemaining(),55.f);
    P->bCinematic=true; P->AdvanceClock(10); TestEqual(TEXT("Cinematic freezes clock"),P->GetRemaining(),55.f); P->bCinematic=false;
    Inventory->TryAddItem(Item,TEXT("RoomA:New")); P->CollectClue(TEXT("Key_1"));
    FHSRoomRoute R; R.RequiredClues.Add(TEXT("Key_1"));
    TestTrue(TEXT("Key unlocks correct stage exit"),P->CanExit(R)); R.Stage=2; TestFalse(TEXT("Wrong stage route rejected"),P->CanExit(R)); R.Stage=1;
    P->RollbackRoom();
    TestTrue(TEXT("Previous-room inventory retained"),Inventory->CollectedPickups.Contains(TEXT("PriorRoom:Keep")));
    TestFalse(TEXT("Current-room pickup rolled back"),Inventory->CollectedPickups.Contains(TEXT("RoomA:New")));
    TestFalse(TEXT("Current-room clue rolled back"),P->HasClue(TEXT("Key_1")));
    TestEqual(TEXT("Time reset"),P->GetRemaining(),60.f);
    P->BeginCountdown(); P->AdvanceClock(3); P->EnterRoom(TEXT("RoomB"),60); P->BeginCountdown(); P->AdvanceClock(7);
    TestEqual(TEXT("Room A clock independent"),P->Remaining[TEXT("RoomA")],57.f); TestEqual(TEXT("Room B clock independent"),P->GetRemaining(),53.f);
    TestTrue(TEXT("Expiry triggers"),P->AdvanceClock(60)); TestFalse(TEXT("Expiry triggers only once"),P->AdvanceClock(1));
    P->RollbackRoom(); TestEqual(TEXT("All clocks reset A"),P->Remaining[TEXT("RoomA")],60.f); TestEqual(TEXT("All clocks reset B"),P->Remaining[TEXT("RoomB")],60.f);
    P->CollectClue(TEXT("Key_1")); R.bAdvanceStage=true; P->CommitExit(R); TestEqual(TEXT("C exit advances stage"),P->Stage,2);
    TestFalse(TEXT("Stage-specific clue does not leak"),P->HasClue(TEXT("Key_1")));
    TestFalse(TEXT("Invalid stage rejected"),P->SetStage(4)); P->SetStage(3); R.Stage=3; R.bFinishAtFinalStage=true; P->CollectClue(TEXT("Key_1")); P->CommitExit(R);
    TestTrue(TEXT("Final stage ends game"),P->bCompleted); TestFalse(TEXT("Completed game cannot travel"),P->CanExit(R));
    Inventory->ResetSession(); return true;
}

namespace
{
class FHSRoomScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0,Trip=0;
    double Started=FPlatformTime::Seconds(),Until=Started+2;
    double FrozenTime=0; float FrozenCountdown=0,SingleSpeed=0;
    FVector StartLocation,PropLocation;
    FString PickupKey;
    TWeakObjectPtr<AHSMovableProp> Prop;
    void Wait(int32 Next,float Seconds) { Step=Next; Until=FPlatformTime::Seconds()+Seconds; }
    void Shot(const TCHAR* Name) { if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification"),Name),true,false); }
    void Key(AHSPlayerController* PC,FKey K,EInputEvent E) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f)); }
public:
    explicit FHSRoomScenario(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>100) { Test->AddError(TEXT("Room scenario timed out")); return true; }
        if(Now<Until) return false;
        auto* W=ProgressionWorld(); auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* C=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr; auto* D=AHSRoomDirector::Find(W);
        if(!C || !D || !D->Rules) return false;
        auto* P=D->Progress(); auto* Inventory=PC->GetSession();
        if(Step==0)
        {
            Inventory->ResetSession(); UGameplayStatics::OpenLevel(W,TEXT("/HorrorSystems/Maps/Test_A")); Wait(1,3); return false;
        }
        if(Step==1)
        {
            Test->TestTrue(TEXT("First entry starts window shot"),P->bCinematic && D->WindowSequence->bPlaying);
            Test->TestTrue(TEXT("Cinematic disables player movement"),PC->IsMoveInputIgnored());
            Test->TestEqual(TEXT("Window shot does not consume room budget"),P->GetRemaining(),60.f);
            Test->TestTrue(TEXT("Window actor remains at authored location"),D->WindowSequence->GetActorLocation().Equals(FVector(-900,-1800,0),.1f));
            Shot(TEXT("WindowCinematic.png")); Wait(15,2.6f); return false;
        }
        if(Step==15)
        {
            auto* Performer=D->WindowSequence->Performer.Get();
            Test->TestNotNull(TEXT("Monster performs outside window"),Performer);
            if(Performer) Test->TestTrue(TEXT("Monster looks into window"),FVector::DotProduct(Performer->GetActorForwardVector(),(D->WindowSequence->Camera->GetComponentLocation()-Performer->GetActorLocation()).GetSafeNormal2D())>.8f);
            Shot(TEXT("WindowMonsterLook.png")); Wait(2,4.4f); return false;
        }
        if(Step==2)
        {
            Test->TestFalse(TEXT("Shot naturally completes"),P->bCinematic);
            Test->TestFalse(TEXT("Shot restores input"),PC->IsMoveInputIgnored());
            Test->TestTrue(TEXT("Initial player is in safe room"),D->IsSafe(C));
            Test->TestEqual(TEXT("Safe area keeps full budget"),P->GetRemaining(),60.f);
            Shot(TEXT("RoomSafe.png")); PC->SetControlRotation(FRotator(0,0,0)); Key(PC,EKeys::W,IE_Pressed); Wait(3,1.8f); return false;
        }
        if(Step==3)
        {
            Key(PC,EKeys::W,IE_Released);
            Test->TestTrue(TEXT("Leaving vestibule starts timer"),P->bTimerRunning && P->GetRemaining()<60);
            Test->TestTrue(TEXT("Safe return barrier enabled"),D->bSafeAreaSealed && D->ReturnBarrier->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
            C->SetActorLocation(FVector(-1280,0,100)); C->ClearMovementModifiers(); Key(PC,EKeys::S,IE_Pressed); Wait(4,.8f); return false;
        }
        if(Step==4)
        {
            Key(PC,EKeys::S,IE_Released); Test->TestTrue(TEXT("Air wall physically prevents returning"),C->GetActorLocation().X>-1370);
            auto* Door=*TActorIterator<AHSPortal>(W); Test->TestTrue(TEXT("Door remains locked without clue"),Door->IsLocked()); Test->TestFalse(TEXT("Locked door rejects travel"),Door->Travel(C));
            C->SetActorLocation(FVector(1950,-700,100)); C->ClearMovementModifiers(); Key(PC,EKeys::W,IE_Pressed); Wait(5,.8f); return false;
        }
        if(Step==5)
        {
            Key(PC,EKeys::W,IE_Released); Test->TestTrue(TEXT("Locked door has physical collision"),C->GetActorLocation().X<2130);
            C->SetActorLocation(FVector(-1150,250,100)); C->ClearMovementModifiers();
            for(TActorIterator<AHSMovableProp> It(W);It;++It) { Prop=*It; break; }
            if(!Prop.IsValid()) { Test->AddError(TEXT("No movable prop")); return true; }
            PropLocation=Prop->GetActorLocation(); PC->SetControlRotation((PropLocation-C->Camera->GetComponentLocation()).Rotation()); Wait(51,.15f); return false;
        }
        if(Step==51) { C->Interact(); Wait(6,.8f); return false; }
        if(Step==61) { C->Interact(); Wait(7,.35f); return false; }
        if(Step==6)
        {
            Test->TestTrue(TEXT("E moves real scene prop"),Prop.IsValid() && Prop->bOpen && FVector::Dist(PropLocation,Prop->GetActorLocation())>50);
            AHSPickup* Pickup=nullptr; for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("Key_1")) { Pickup=*It; break; }
            if(!Pickup) { Test->AddError(TEXT("No stage clue")); return true; }
            PickupKey=Pickup->GetPersistentKey(); C->SetActorLocation(FVector(-1100,-270,100)); C->ClearMovementModifiers(); PC->SetControlRotation((Pickup->GetActorLocation()-C->Camera->GetComponentLocation()).Rotation()); Wait(61,.15f); return false;
        }
        if(Step==7)
        {
            Test->TestTrue(TEXT("First-person pickup pose plays"),C->PickupPoseAlpha>.5f);
            Test->TestTrue(TEXT("Pickup montage is bound to one valid slot"),C->PickupMontage && C->PickupMontage->SlotAnimTracks.Num()==1 && C->PickupMontage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num()==1);
            Test->TestTrue(TEXT("Pickup montage actually plays"),C->GetMesh()->GetAnimInstance()->Montage_IsPlaying(C->PickupMontage));
            Test->TestTrue(TEXT("Real pickup grants clue"),P->HasClue(TEXT("Key_1")) && Inventory->CollectedPickups.Contains(PickupKey));
            Shot(TEXT("FirstPersonPickup.png")); Wait(8,.5f); return false;
        }
        if(Step==8)
        {
            auto* Door=*TActorIterator<AHSPortal>(W); Test->TestFalse(TEXT("Clue unlocks door"),Door->IsLocked());
            PC->InspectSelected(); Test->TestTrue(TEXT("Clue inspection pauses world"),PC->IsInspecting());
            FrozenTime=W->GetTimeSeconds(); FrozenCountdown=P->GetRemaining(); Shot(TEXT("RoomInspection.png")); Wait(9,.8f); return false;
        }
        if(Step==9)
        {
            Test->TestEqual(TEXT("Inspection freezes world"),W->GetTimeSeconds(),FrozenTime); Test->TestEqual(TEXT("Inspection freezes countdown"),P->GetRemaining(),FrozenCountdown);
            PC->CloseInspection(); P->Remaining.FindOrAdd(P->ActiveRoom)=.05f; Wait(10,2); return false;
        }
        if(Step==10)
        {
            Test->TestTrue(TEXT("Timeout returns to current safe room"),D->IsSafe(C) && P->ActiveRoom==TEXT("RoomA"));
            Test->TestEqual(TEXT("Timeout resets budget"),P->GetRemaining(),60.f); Test->TestFalse(TEXT("Timeout reopens safe area"),D->bSafeAreaSealed);
            Test->TestFalse(TEXT("Timeout removes acquired item"),Inventory->CollectedPickups.Contains(PickupKey)); Test->TestFalse(TEXT("Timeout clears acquired clue"),P->HasClue(TEXT("Key_1")));
            Test->TestFalse(TEXT("Timeout does not replay first shot"),P->bCinematic);
            bool Respawned=false; for(TActorIterator<AHSPickup> It(W);It;++It) if(It->GetPersistentKey()==PickupKey) Respawned=true;
            Test->TestTrue(TEXT("Rolled-back pickup respawns"),Respawned);
            Shot(TEXT("RoomTimeoutReset.png")); Wait(11,.1f); return false;
        }
        if(Step==11)
        {
            const int32 ExpectedStage=1+Trip/3; const FName ExpectedRoom(*FString::Printf(TEXT("Room%c"),TEXT('A')+Trip%3));
            Test->TestEqual(TEXT("Cycle reaches expected stage"),P->Stage,ExpectedStage); Test->TestEqual(TEXT("Cycle reaches expected room"),P->ActiveRoom,ExpectedRoom);
            D->FinishIntro();
            AHSPickup* Pickup=nullptr; for(TActorIterator<AHSPickup> It(W);It;++It) if(!It->ClueId.IsNone()) { Pickup=*It; break; }
            if(!Pickup) { Test->AddError(TEXT("Missing stage pickup")); return true; }
            C->SetActorLocation(Pickup->GetActorLocation()+FVector(-120,0,30)); C->ClearMovementModifiers();
            Test->TestTrue(TEXT("Each room's key can be collected"),Pickup->TryPickup(C));
            auto* Door=*TActorIterator<AHSPortal>(W); Test->TestFalse(TEXT("Current route unlocked"),Door->IsLocked());
            Test->TestTrue(TEXT("Stage-configured exit works"),Door->Travel(C));
            ++Trip; if(Trip==9) Wait(12,.2f); else Wait(11,2); return false;
        }
        if(Step==12)
        {
            Test->TestTrue(TEXT("Third cycle completes game"),P->bCompleted && D->EndingTime>0 && PC->IsMoveInputIgnored());
            Shot(TEXT("EndingWhiteFlash.png")); Wait(13,2); return false;
        }
        if(Step==13)
        {
            Test->TestTrue(TEXT("Completed timer stays stopped"),!P->bTimerRunning);
            Shot(TEXT("GameEnding.png")); return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRoomsTest,"HorrorSystems.Progression.RoomFlow",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSRoomsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FHSRoomScenario(this)); return true; }
#endif


