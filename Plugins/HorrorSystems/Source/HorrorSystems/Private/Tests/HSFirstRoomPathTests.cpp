#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSStoryActors.h"
#include "HSProgression.h"
#include "HSWorldState.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FHSFirstRoomPath : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHSCharacter> Player;
    double Started=-1;
    int32 Phase=0;
    bool DoorOpened=false;
    bool LoadedFreshRoom=false;
    double LoadStarted=0;
public:
    explicit FHSFirstRoomPath(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        if(!W) {Test->AddError(TEXT("No game world"));return true;}
        if(!LoadedFreshRoom)
        {
            if(auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0))) PC->GetSession()->ResetSession();
            LoadedFreshRoom=true;LoadStarted=FPlatformTime::Seconds();UGameplayStatics::OpenLevel(W,TEXT("/HorrorSystems/Maps/Basic_roomA"));return false;
        }
        if(Started<0)
        {
            auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0));
            Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
            auto* Room=AHSRoomDirector::Find(W);
            if(!Room || !Player.IsValid())
            {if(FPlatformTime::Seconds()-LoadStarted>30) {Test->AddError(TEXT("Room or player missing"));return true;}return false;}
            Test->TestEqual(TEXT("Fresh game starts in the first room"),Room->Rules->RoomId,FName(TEXT("RoomA")));
            Test->TestEqual(TEXT("Fresh game starts in stage one"),Room->Progress()->Stage,1);
            Room->FinishIntro();PC->SetGameplayLocked(false);Player->ClearMovementModifiers();
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;if(It->GetController()) It->GetController()->StopMovement();}
            Started=W->GetTimeSeconds();
        }
        if(!Player.IsValid()) {Test->AddError(TEXT("Player lost"));return true;}
        const FVector Direction=Phase==0?FVector(0,1,0):FVector(-1,0,0);
        if(!DoorOpened && Player->GetActorLocation().Y>-830)
        {if(auto* Story=Cast<AHSStoryDirector>(AHSRoomDirector::Find(W));Story && Story->SafeDoor) DoorOpened=Story->SafeDoor->Use(Player.Get());}
        Player->AddMovementInput(Direction,1.f);
        if(Phase==0 && Player->GetActorLocation().Y>300.f)
        {Phase=1;Started=W->GetTimeSeconds();Player->ConsumeMovementInputVector();Player->GetCharacterMovement()->StopMovementImmediately();}
        if(Phase==1 && Player->GetActorLocation().X<-700.f)
        {Test->AddInfo(FString::Printf(TEXT("First-room main area reached by walking: %s"),*Player->GetActorLocation().ToString()));Player->GetCharacterMovement()->StopMovementImmediately();return true;}
        if(W->GetTimeSeconds()-Started>8.f)
        {
            FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Player.Get());
            W->SweepSingleByChannel(Hit,Player->GetActorLocation(),Player->GetActorLocation()+Direction*120.f,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(42,92),Query);
            Test->AddError(FString::Printf(TEXT("Cannot reach first-room main area: phase=%d player=%s blocker=%s impact=%s"),Phase,*Player->GetActorLocation().ToString(),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString()));return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSFirstRoomPathTest,"HorrorSystems.Progression.FirstRoomPath",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSFirstRoomPathTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FHSFirstRoomPath(this));return true; }
#endif
