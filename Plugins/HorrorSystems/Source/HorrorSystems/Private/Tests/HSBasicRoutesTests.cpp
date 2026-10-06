#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FBasicRoutes : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int Trip=0,Step=0;
    double Started=FPlatformTime::Seconds(),Until=Started+3;
    bool bCombined=false;
    FString ExpectedRoom=TEXT("RoomA");
public:
    explicit FBasicRoutes(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>80) {Test->AddError(TEXT("Basic room route scenario timed out"));return true;}
        if(FPlatformTime::Seconds()<Until) return false;
        UWorld* W=nullptr;for(auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        auto* Room=AHSRoomDirector::Find(W);
        if(!Room || !Player) return false;
        auto* P=Room->Progress();
        for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->GetCharacterMovement()->StopMovementImmediately();}
        if(Step==0)
        {
            if(Trip==0) bCombined=UGameplayStatics::GetCurrentLevelName(W,true)==TEXT("Basic_roomABC");
            Room->FinishIntro();PC->SetGameplayLocked(false);
            Test->TestEqual(TEXT("Arrival chooses correct room director"),Room->Rules->RoomId.ToString(),ExpectedRoom);
            Test->TestEqual(TEXT("Stage follows three room cycles"),P->Stage,Trip/3+1);
            Test->TestTrue(TEXT("Arrival is inside next safe room"),Room->IsSafe(Player));
            Test->TestFalse(TEXT("Arrival countdown is stopped"),P->bTimerRunning);
            Test->TestEqual(TEXT("Arrival gets full room time"),P->GetRemaining(),60.f);
            AHSPortal* Door=nullptr;
            for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules==Room->Rules) {Door=*It;break;}
            if(!Door) {Test->AddError(TEXT("Room exit missing"));return true;}
            Test->TestTrue(TEXT("Exit is locked before current room clue"),Door->IsLocked());
            AHSPickup* Key=nullptr;
            for(TActorIterator<AHSPickup> It(W);It;++It) if(It->MinimumStage==P->Stage && It->ClueId==FName(*FString::Printf(TEXT("Key_%d"),P->Stage)) && It->GetActorLabel().Contains(Room->Rules->RoomId.ToString().Right(1)+TEXT("_"))) {Key=*It;break;}
            if(!Key) {Test->AddError(TEXT("Current stage key missing"));return true;}
            Player->SetActorLocation(Key->GetActorLocation()+FVector(110,0,60));Room->Tick(.01f);
            Test->TestTrue(TEXT("Current stage clue can be picked up"),Key->TryPickup(Player));
            Test->TestFalse(TEXT("Collecting clue unlocks exit"),Door->IsLocked());
            Step=1;Until=FPlatformTime::Seconds()+.9;return false;
        }
        AHSPortal* Door=nullptr;
        for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules==Room->Rules) {Door=*It;break;}
        if(!Door) return false;
        Test->TestTrue(TEXT("Configured exit accepts native travel"),Door->Travel(Player));
        if(Trip==8)
        {
            Test->TestTrue(TEXT("Final cycle completes game"),P->bCompleted);
            Test->TestTrue(TEXT("Final exit starts white-flash ending"),Room->EndingTime>0.f);
            Test->TestTrue(TEXT("Ending locks gameplay"),PC->IsGameplayLocked());
            return true;
        }
        ++Trip;ExpectedRoom=FString::Printf(TEXT("Room%c"),TEXT('A')+Trip%3);
        Step=0;Until=FPlatformTime::Seconds()+(bCombined?.5:2.5);return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicRoutesTest,"HorrorSystems.Basic.RoomRoutes",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FBasicRoutesTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FBasicRoutes(this));return true;}
#endif
