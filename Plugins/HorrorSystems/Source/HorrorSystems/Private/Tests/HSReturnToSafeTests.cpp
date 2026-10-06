#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "Engine/Engine.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSReturnToSafeTest,"HorrorSystems.Progression.ReturnToSafe",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSReturnToSafeTest::RunTest(const FString&)
{
    UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
    auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
    if(!Player || !Room) {AddError(TEXT("Configured room required"));return false;}
    Room->FinishIntro();PC->SetGameplayLocked(false);
    const float Z=Player->GetActorLocation().Z;
    Player->SetActorLocation(FVector(-200,-400,Z));Room->Tick(.01f);
    TestTrue(TEXT("Leaving starts the sixty-second clock"),Room->Progress()->bTimerRunning);
    TestEqual(TEXT("Leaving blocks return before the objective"),Room->ReturnBarrier->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics);
    Room->Progress()->CollectClue(TEXT("Key_1"));Room->Tick(.01f);
    TestEqual(TEXT("Obtaining the required item removes the return air wall"),Room->ReturnBarrier->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Countdown continues until returning safely"),Room->Progress()->bTimerRunning);
    FHitResult Hit;Player->SetActorLocation(FVector(-200,-850,Z),true,&Hit);Room->Tick(.01f);
    TestTrue(TEXT("Player can physically reenter the safe room"),Room->IsSafe(Player));
    TestFalse(TEXT("Returning with the objective stops countdown"),Room->Progress()->bTimerRunning);
    const float Remaining=Room->Progress()->GetRemaining();Room->Tick(2.f);
    TestEqual(TEXT("Ready safe room does not spend time"),Room->Progress()->GetRemaining(),Remaining);
    return true;
}
#endif
