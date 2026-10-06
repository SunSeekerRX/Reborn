#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSSceneInteractions.h"
#include "HSAI.h"
#include "HSItemData.h"
#include "HSWorldActors.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UWorld* TestWorld() {for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) return C.World();return nullptr;}
class FTitleRoundTrip : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;double Started=FPlatformTime::Seconds();bool Opaque=false;
public:
    explicit FTitleRoundTrip(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>45) {Test->AddError(TEXT("Title round trip timed out"));return true;}
        UWorld* W=TestWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        if(!PC || FPlatformTime::Seconds()-Started<1) return false;
        auto* S=PC->GetSession();Opaque|=S->WhiteTravelAlpha>=.999f;
        if(Step==0)
        {
            Test->TestTrue(TEXT("Boot opens title screen"),S->bTitleScreen);
            Test->TestNotNull(TEXT("Pixel Mobius texture is loaded"),PC->TitleTexture.Get());
            Test->TestNull(TEXT("Title does not spawn gameplay character"),PC->GetPawn());
            Test->TestTrue(TEXT("Title shows mouse cursor"),PC->bShowMouseCursor);
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/RebornTitle.png")),true,false);
            Step=1;return false;
        }
        if(Step==1)
        {
            Test->TestTrue(TEXT("Start action begins white transition"),S->BeginMenuTravel(true));
            Test->TestFalse(TEXT("Double start is ignored"),S->BeginMenuTravel(true));Step=2;return false;
        }
        if(Step==2 && !S->bTitleScreen && !S->bWhiteTransition)
        {
            auto* Room=AHSRoomDirector::Find(W);auto* Player=Cast<AHSCharacter>(PC->GetPawn());
            if(!Room || !Player) return false;
            Test->TestTrue(TEXT("Start was fully covered by white"),Opaque);
            Test->TestTrue(TEXT("Start enters A safe room"),Room->IsSafe(Player));
            Test->TestEqual(TEXT("New game starts stage one"),Room->Progress()->Stage,1);
            Test->TestEqual(TEXT("New game starts with full health"),S->Lives,3);
            Room->Progress()->bCompleted=true;Room->BeginEnding();Opaque=false;Step=3;return false;
        }
        if(Step==3 && S->bTitleScreen && !S->bWhiteTransition)
        {
            Test->TestTrue(TEXT("Completion was fully covered by white"),Opaque);
            Test->TestTrue(TEXT("Same title is interactive on return"),PC->bShowMouseCursor);
            Test->TestTrue(TEXT("Can start again from returned title"),S->BeginMenuTravel(true));Step=4;return false;
        }
        if(Step==4 && !S->bTitleScreen && !S->bWhiteTransition)
        {
            auto* Room=AHSRoomDirector::Find(W);if(!Room) return false;
            Test->TestFalse(TEXT("Second start clears completed state"),Room->Progress()->bCompleted);
            Test->TestEqual(TEXT("Second start resets stage"),Room->Progress()->Stage,1);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSTitleTest,"HorrorSystems.UI.TitleRoundTrip",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSTitleTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FTitleRoundTrip(this));return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRecoveryTest,"HorrorSystems.Combat.SafeRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSRecoveryTest::RunTest(const FString&)
{
    UWorld* W=TestWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
    if(!Player || !Room) {AddError(TEXT("Gameplay map required"));return false;}
    Room->FinishIntro();PC->SetGameplayLocked(false);auto* S=PC->GetSession();auto* P=Room->Progress();
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Monster=W->SpawnActor<AHSMonster>(AHSMonster::StaticClass(),FVector(-700,500,300),FRotator::ZeroRotator,Spawn);Monster->bCanDamagePlayer=true;
    auto* Shelf=W->SpawnActor<AHSCollapsingObstacle>();
    for(int32 Stage=1;Stage<=3;++Stage)
    {
        P->SetStage(Stage);Room->PrepareEntry();Room->Tick(0);S->Lives=3;
        Shelf->StartCollapse();
        TestEqual(TEXT("Collapse occurs only in stage three"),Shelf->bFalling,Stage==3);
        if(Stage==3) Shelf->Tick(2.f);
        P->CollectClue(TEXT("RecoveryEvidence"));P->Remaining[Room->Rules->RoomId]=37.f;
        auto* Item=LoadObject<UHSItemData>(nullptr,TEXT("/HorrorSystems/Items/DA_Key.DA_Key"));
        S->TryAddItem(Item,FString::Printf(TEXT("RecoveryItem_%d"),Stage));
        const auto SlotsBefore=S->Slots;const UWorld* WorldBefore=W;
        Player->SetActorLocation(FVector(-700,300,300));Room->Tick(0);
        for(int32 Hit=0;Hit<3;++Hit) {Player->HitProtectionRemaining=0;TestTrue(TEXT("Hit is accepted"),Player->ReceiveMonsterContact(Monster));TestEqual(TEXT("Three hits restore health instead of game over"),S->Lives,Hit<2?2-Hit:3);}
        TestTrue(TEXT("Recovery returns to last safety"),Player->GetActorLocation().Equals(S->RecoveryTransform.GetLocation(),1));
        TestEqual(TEXT("Death does not change stage"),P->Stage,Stage);
        TestTrue(TEXT("Death preserves acquired clue"),P->HasClue(TEXT("RecoveryEvidence")));
        TestTrue(TEXT("Death keeps current map instance"),WorldBefore==Player->GetWorld());
        for(int32 I=0;I<SlotsBefore.Num();++I) TestEqual(TEXT("Death preserves each inventory slot"),S->Slots[I].Item.Get(),SlotsBefore[I].Item.Get());
        TestEqual(TEXT("Death preserves remaining time"),P->GetRemaining(),37.f);
        TestFalse(TEXT("Recovery safety pauses clock"),P->bTimerRunning);
        TestFalse(TEXT("Controls recover immediately"),PC->IsGameplayLocked());
        if(Stage==3) TestTrue(TEXT("Death preserves collapsed furniture"),Shelf->bCollapsed);
        TestTrue(TEXT("Short protection prevents immediate repeated hit"),Player->HitProtectionRemaining>0);
        Player->SetActorLocation(FVector(-200,-400,300));Room->Tick(0);
        TestTrue(TEXT("Leaving recovery safety resumes clock"),P->bTimerRunning);
        TestTrue(TEXT("No required clue still seals safety"),Room->bSafeAreaSealed);
    }
    P->SetStage(1);Room->PrepareEntry();Room->Tick(0);P->CollectClue(TEXT("Key_1"));
    Player->SetActorLocation(FVector(-700,300,300));Room->Tick(0);S->Lives=3;
    for(int32 Hit=0;Hit<3;++Hit) {Player->HitProtectionRemaining=0;Player->ReceiveMonsterContact(Monster);}
    TestTrue(TEXT("Recovery with objective counts as returning to safety"),Room->bSafeTravelReady);
    for(TActorIterator<AHSPortal> It(W);It;++It) if(It->bRequiresSafeReturn) {It->Tick(0);TestFalse(TEXT("Completed safe door stays unlocked after recovery"),It->IsLocked());}
    TestFalse(TEXT("Recovery point does not automatically trigger next room"),S->bWhiteTransition);
    Shelf->Destroy();Monster->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSLatestCheckpointTest,"HorrorSystems.Combat.LatestSafetyRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSLatestCheckpointTest::RunTest(const FString&)
{
    UWorld* W=TestWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
    if(!Player || !Room || !Room->bFinalEscapeMode) {AddError(TEXT("ABC required"));return false;}
    auto* P=Room->Progress();const auto OriginalClues=P->Clues;const auto InitialPose=Player->GetActorTransform();P->CollectClue(TEXT("Key_3"));Room->Tick(0);
    AHSRecoveryCheckpoint* Check=nullptr;for(TActorIterator<AHSRecoveryCheckpoint> It(W);It;++It) {Check=*It;break;}
    if(!Check) {AddError(TEXT("Checkpoint missing"));return false;}
    Player->SetActorTransform(Check->SpawnTransform);Check->Tick(0);
    auto* S=PC->GetSession();TestTrue(TEXT("Entered safety becomes latest recovery point"),S->RecoveryTransform.Equals(Check->SpawnTransform));
    Player->SetActorLocation(Check->SpawnTransform.GetLocation()+FVector(0,400,0));Room->Tick(0);S->Lives=0;
    TestTrue(TEXT("Recovery uses current checkpoint without loading map"),S->RecoverAtSafety());
    TestTrue(TEXT("Returned to latest checkpoint rather than A start"),Player->GetActorLocation().Equals(Check->SpawnTransform.GetLocation(),1));
    TestTrue(TEXT("Final information remains acquired"),P->HasClue(TEXT("Key_3")));TestEqual(TEXT("Full health"),S->Lives,3);
    Player->SetActorTransform(InitialPose);P->Clues=OriginalClues;Room->PrepareEntry();Room->Tick(0);return true;
}
#endif
