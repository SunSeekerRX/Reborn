#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FHSSafeExitWalk : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHSCharacter> Player;
    TWeakObjectPtr<AHSRoomDirector> Room;
    TArray<TWeakObjectPtr<AHSRoomDirector>> Rooms;
    int32 RoomIndex=0;
    double Started=-1;
    float ExitY=0,Sign=1,LowestZ=MAX_flt;
public:
    explicit FHSSafeExitWalk(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        if(!W) { Test->AddError(TEXT("No game world")); return true; }
        if(Started<0)
        {
            auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0));
            Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
            if(Rooms.IsEmpty())
            {
                Rooms.Add(AHSRoomDirector::Find(W));
                for(TActorIterator<AHSRoomDirector> It(W);It;++It) if(It->bUseRoomBounds && *It!=Rooms[0].Get()) Rooms.Add(*It);
            }
            Room=Rooms[RoomIndex];
            if(!Player.IsValid() || !Room.IsValid()) { Test->AddError(TEXT("Basic room player or director missing")); return true; }
            if(RoomIndex>0)
            {
                bool SpawnFound=false;
                for(TActorIterator<APlayerStart> It(W);It;++It) if(It->PlayerStartTag==Room->SafeSpawnTag)
                {
                    Player->GetCharacterMovement()->StopMovementImmediately(); Player->ConsumeMovementInputVector();
                    Player->SetActorLocation(It->GetActorLocation()); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                    SpawnFound=true; break;
                }
                if(!SpawnFound) {Test->AddError(TEXT("Safe-room PlayerStart missing"));return true;}
                Room->PrepareEntry(); Room->Tick(.001f);
            }
            Room->FinishIntro(); PC->SetGameplayLocked(false); Player->ClearMovementModifiers();
            for(TActorIterator<AHSMonster> It(W);It;++It) { It->bCanDamagePlayer=false; It->bCinematicActor=true; if(It->GetController()) It->GetController()->StopMovement(); }
            Sign=Room->RoomBounds->GetComponentLocation().Y>Room->SafeArea->GetComponentLocation().Y?1.f:-1.f;
            ExitY=Room->SafeArea->GetComponentLocation().Y+Sign*300.f;
            float Previous=0; bool HadPrevious=false;
            FCollisionQueryParams Query; Query.AddIgnoredActor(Player.Get());
            for(int32 I=0;I<=18;++I)
            {
                const FVector Start(-200,ExitY+Sign*(-450+I*50),550);
                FHitResult Floor;
                const bool Found=W->LineTraceSingleByChannel(Floor,Start,Start-FVector(0,0,1600),ECC_Visibility,Query);
                Test->AddInfo(FString::Printf(TEXT("Exit floor y=%.1f hit=%d z=%.1f actor=%s"),Start.Y,Found,Floor.ImpactPoint.Z,*GetNameSafe(Floor.GetActor())));
                Test->TestTrue(TEXT("Exit walkway has a floor under every sample"),Found);
                if(Found && HadPrevious) Test->TestTrue(TEXT("Exit floor has no abrupt height drop"),FMath::Abs(Floor.ImpactPoint.Z-Previous)<=45.f);
                Previous=Floor.ImpactPoint.Z; HadPrevious=Found;
            }
            Started=W->GetTimeSeconds();
        }
        if(!Player.IsValid()) {Test->AddError(TEXT("Player lost during safe exit"));return true;}
        LowestZ=FMath::Min(LowestZ,Player->GetActorLocation().Z);
        Player->AddMovementInput(FVector(0,Sign,0),1.f);
        if((Player->GetActorLocation().Y-ExitY)*Sign>350.f)
        {
            Player->GetCharacterMovement()->StopMovementImmediately();
            Test->TestTrue(TEXT("Walking out never drops below the room floor"),LowestZ>150.f);
            Test->TestTrue(TEXT("Walking out starts countdown"),Room->bSafeAreaSealed);
            Test->AddInfo(FString::Printf(TEXT("Actual walk reached %s minimum z=%.1f"),*Player->GetActorLocation().ToString(),LowestZ));
            if(++RoomIndex<Rooms.Num()) {Started=-1;LowestZ=MAX_flt;return false;}
            return true;
        }
        if(W->GetTimeSeconds()-Started>8.f)
        {Test->AddError(FString::Printf(TEXT("Safe exit walk blocked or fell: %s minimum z=%.1f"),*Player->GetActorLocation().ToString(),LowestZ));return true;}
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSSafeExitFloorTest,"HorrorSystems.Progression.SafeExitFloor",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSSafeExitFloorTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FHSSafeExitWalk(this)); return true; }
#endif
