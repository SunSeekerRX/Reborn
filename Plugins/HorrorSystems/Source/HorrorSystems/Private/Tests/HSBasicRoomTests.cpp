#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSBasicRoomSafetyTest,
    "HorrorSystems.Progression.BasicRoomSafety",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHSBasicRoomSafetyTest::RunTest(const FString&)
{
    UWorld* World=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts())
        if(Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE) World=Context.World();
    auto* PC=World?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(World,0)):nullptr;
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
    auto* Director=AHSRoomDirector::Find(World);
    if(!Player || !Director || !Director->Rules)
    { AddError(TEXT("Run this test in either Basic_room map")); return false; }
    Director->FinishIntro(); PC->SetGameplayLocked(false);
    auto* Progress=Director->Progress();
    TestEqual(TEXT("Basic room rules bound"),Director->Rules->RoomId,FName(TEXT("RoomA")));
    TestTrue(TEXT("Player spawns inside safe room"),Director->IsSafe(Player));
    TestEqual(TEXT("Full sixty second budget"),Progress->GetRemaining(),60.f);
    TestFalse(TEXT("Countdown initially stopped"),Progress->bTimerRunning);
    Director->Tick(5.f);
    TestEqual(TEXT("Five seconds in safe room costs no time"),Progress->GetRemaining(),60.f);
    TestEqual(TEXT("Barrier initially inactive"),Director->ReturnBarrier->GetCollisionEnabled(),ECollisionEnabled::NoCollision);

    Player->GetCharacterMovement()->StopMovementImmediately();
    const float GroundedZ=Player->GetActorLocation().Z;
    FHitResult Hit;
    Player->SetActorLocation(FVector(-200,-610,GroundedZ),true,&Hit);
    TestTrue(TEXT("Player can approach door inside safe room"),Player->GetActorLocation().Y>-620.f);
    Director->Tick(.5f);
    TestFalse(TEXT("Capsule overlapping safe area does not seal it"),Director->bSafeAreaSealed);
    TestEqual(TEXT("Door threshold still has full time"),Progress->GetRemaining(),60.f);

    Player->SetActorLocation(FVector(-200,-400,GroundedZ),true,&Hit);
    TestTrue(TEXT("Real capsule sweep passes through new doorway"),Player->GetActorLocation().Y>-420.f);
    Director->Tick(.5f);
    TestTrue(TEXT("Leaving seals safe room"),Director->bSafeAreaSealed);
    TestTrue(TEXT("Leaving starts countdown"),Progress->bTimerRunning);
    TestTrue(TEXT("Time decreases after exit"),Progress->GetRemaining()<60.f);
    TestEqual(TEXT("Air wall physically enabled"),Director->ReturnBarrier->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics);

    Player->SetActorLocation(FVector(-200,-800,GroundedZ),true,&Hit);
    TestTrue(TEXT("Air wall blocks returning capsule"),Hit.bBlockingHit);
    TestTrue(TEXT("Player remains outside safe area"),Player->GetActorLocation().Y>-600.f);
    const float Remaining=Progress->GetRemaining();
    Director->Tick(2.f);
    TestTrue(TEXT("Attempted return does not stop countdown"),Progress->GetRemaining()<Remaining);
    return true;
}
#endif
