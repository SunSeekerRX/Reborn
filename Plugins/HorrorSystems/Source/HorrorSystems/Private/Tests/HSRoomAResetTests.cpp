#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSSceneInteractions.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FRoomAReset : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Until=FPlatformTime::Seconds()+1;
    double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<UWorld> OldWorld;
    FString KeyName;
public:
    explicit FRoomAReset(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>30) {Test->AddError(TEXT("RoomA timeout reset did not complete"));return true;}
        if(FPlatformTime::Seconds()<Until) return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
        if(!Player || !Room) return false;
        if(Step==0)
        {
            Room->FinishIntro();PC->SetGameplayLocked(false);
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;}
            AHSPickup* Key=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("Key_1")) {Key=*It;break;}
            if(!Key) {Test->AddError(TEXT("RoomA stage-one key missing"));return true;}
            KeyName=Key->GetName();Player->SetActorLocation(Key->GetActorLocation()+FVector(110,0,60));Room->Tick(.01f);
            Test->TestTrue(TEXT("RoomA clue can actually be picked up"),Key->TryPickup(Player));
            Test->TestTrue(TEXT("Clue acquisition tracked"),Room->Progress()->HasClue(TEXT("Key_1")));
            for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It) {It->StartCollapse();It->Tick(2.f);Test->TestTrue(TEXT("Shelf collapsed before reset"),It->bCollapsed);}
            ++Step;Until=FPlatformTime::Seconds()+1;return false;
        }
        if(Step==1)
        {
            PC->CloseInspection();Player->CancelAutoInspection();OldWorld=W;
            Room->Progress()->BeginCountdown();
            Test->TestTrue(TEXT("Sixty-second budget expires"),Room->Progress()->AdvanceClock(61));
            Room->HandleTimeout();++Step;Until=FPlatformTime::Seconds()+2;return false;
        }
        if(OldWorld.Get()==W) return false;
        Test->TestEqual(TEXT("Timeout reloads RoomA"),UGameplayStatics::GetCurrentLevelName(W,true),FString(TEXT("Basic_roomA")));
        Test->TestTrue(TEXT("Timeout returns to safe spawn"),Room->IsSafe(Player));
        Test->TestEqual(TEXT("Timeout restores full room budget"),Room->Progress()->GetRemaining(),60.f);
        Test->TestFalse(TEXT("Safe-room timer remains stopped"),Room->Progress()->bTimerRunning);
        Test->TestFalse(TEXT("Timeout clears this attempt's clue"),Room->Progress()->HasClue(TEXT("Key_1")));
        Test->TestNull(TEXT("Timeout removes acquired item from hotbar"),PC->GetSession()->GetSelectedItem());
        bool RestoredKey=false;for(TActorIterator<AHSPickup> It(W);It;++It) RestoredKey|=It->ClueId==TEXT("Key_1");
        Test->TestTrue(TEXT("Key is restored for another attempt"),RestoredKey);
        for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It) Test->TestFalse(TEXT("Shelf resets upright for another attempt"),It->bCollapsed || It->bFalling);
        Test->TestFalse(TEXT("Safe-room barrier is reopened after reset"),Room->bSafeAreaSealed);
        Test->TestFalse(TEXT("Player input is restored after timeout"),PC->IsGameplayLocked());
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRoomAResetTest,"HorrorSystems.Basic.RoomATimeoutReset",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSRoomAResetTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FRoomAReset(this));return true;}
#endif
