#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FSafeTravelScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Trip=0,Step=0;
    double Until=FPlatformTime::Seconds()+1,Started=FPlatformTime::Seconds();
    TWeakObjectPtr<UWorld> Departure;
    FString DestinationPackage;
    bool OldOpaque=false,NewOpaque=false;
    float SafeZ=0;
    TWeakObjectPtr<AHSPortal> Door;
    bool SourceCaptured=false,DestinationCaptured=false;
    void Screenshot(const TCHAR* Name)
    {if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification"),Name),true,false);}
public:
    explicit FSafeTravelScenario(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>110) {Test->AddError(TEXT("Safe-travel scenario timed out"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
        if(!Player || !Room) return false;
        auto* State=PC->GetSession();auto* P=Room->Progress();
        if(Step==2)
        {
            if(Departure.Get()==W) OldOpaque|=State->WhiteTravelAlpha>=.999f;
            else NewOpaque|=State->WhiteTravelAlpha>=.999f;
            if(Trip==0 && State->WhiteTravelAlpha>=.999f)
            {
                if(Departure.Get()==W && !SourceCaptured) {SourceCaptured=true;Screenshot(TEXT("SafeTravelSourceWhite.png"));}
                if(Departure.Get()!=W && !DestinationCaptured) {DestinationCaptured=true;Screenshot(TEXT("SafeTravelArrivalWhite.png"));}
            }
        }
        if(FPlatformTime::Seconds()<Until) return false;
        if(Step==0)
        {
            if(Trip==1) Test->TestTrue(TEXT("First entry in B plays its cinematic"),Room->WindowSequence && Room->WindowSequence->bPlaying);
            Room->FinishIntro();PC->SetGameplayLocked(false);
            Test->AddInfo(FString::Printf(TEXT("Safe arrival %d: map=%s room=%s stage=%d time=%.1f"),Trip,*W->GetOutermost()->GetName(),*Room->Rules->RoomId.ToString(),P->Stage,P->GetRemaining()));
            const FName ExpectedRooms[]={TEXT("RoomA"),TEXT("RoomB"),TEXT("RoomC"),TEXT("RoomB"),TEXT("RoomA"),TEXT("RoomA")};
            Test->TestEqual(TEXT("Arrival selects the intended safe room"),Room->Rules->RoomId,ExpectedRooms[Trip]);
            Test->TestEqual(TEXT("Correct three-stage order"),P->Stage,Trip<2?1:Trip<5?2:3);
            Test->TestTrue(TEXT("Arrival is inside the safe room"),Room->IsSafe(Player));
            Test->TestFalse(TEXT("Arrival countdown is stopped"),P->bTimerRunning);
            Test->TestEqual(TEXT("Arrival has sixty seconds"),P->GetRemaining(),60.f);
            Test->TestFalse(TEXT("Arrival has no remaining white transition"),State->bWhiteTransition);
            if(Trip==5)
            {
                Test->TestTrue(TEXT("Third stage uses one continuous escape objective"),Room->bFinalEscapeMode);
                Test->AddInfo(TEXT("Five real transfers completed: A1 -> B1 [return advances stage 2] -> C2 -> B2 -> A2 [return advances stage 3] -> ABC3"));return true;
            }
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;if(It->GetController()) It->GetController()->StopMovement();}
            Door=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It) if(It->bWhiteLightTravel && It->RoomRules==Room->Rules) {Door=*It;break;}
            if(!Door.IsValid()) {Test->AddError(TEXT("White-light safe door missing"));return true;}
            Test->TestTrue(TEXT("Door is locked before getting the objective"),Door->IsLocked());
            SafeZ=Player->GetActorLocation().Z;
            AHSPickup* Key=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==FName(*FString::Printf(TEXT("Key_%d"),P->Stage))) {Key=*It;break;}
            if(!Key) {Test->AddError(TEXT("Required objective pickup missing"));return true;}
            Player->SetActorLocation(Key->GetActorLocation()+FVector(110,0,60));Room->Tick(.01f);
            Test->TestTrue(TEXT("Leaving starts countdown"),P->bTimerRunning);
            Test->TestTrue(TEXT("Air wall blocks returning without the objective"),Room->bSafeAreaSealed);
            Test->TestTrue(TEXT("Actual objective pickup succeeds"),Key->TryPickup(Player));Player->CancelAutoInspection();Room->Tick(.01f);
            Test->TestTrue(TEXT("Objective changes task to return to safety"),Room->bObjectiveAcquired);
            Test->TestFalse(TEXT("Objective removes the air wall"),Room->bSafeAreaSealed);
            Test->TestTrue(TEXT("Door remains locked until returning"),Door->IsLocked());
            Test->TestFalse(TEXT("Cannot travel directly from the pickup location"),Door->Travel(Player));
            Player->SetActorLocation(FVector(-200,-400,SafeZ));FHitResult Hit;
            Player->SetActorLocation(FVector(-200,-850,SafeZ),true,&Hit);Room->Tick(.01f);
            Test->TestTrue(TEXT("Actual capsule can return into safety"),Room->IsSafe(Player));
            Test->TestTrue(TEXT("Returning makes the white door ready"),Room->bSafeTravelReady);
            Test->TestEqual(TEXT("Stage changes immediately on safe return at chapter boundaries"),P->Stage,Trip<1?1:Trip<4?2:3);
            Test->TestFalse(TEXT("Returning stops countdown"),P->bTimerRunning);
            Door->Tick(0);Test->TestFalse(TEXT("White door is unlocked"),Door->IsLocked());
            Test->TestTrue(TEXT("White plane becomes visible"),Door->Marker->IsVisible());
            PC->SetControlRotation((Door->Marker->GetComponentLocation()-Player->Camera->GetComponentLocation()).Rotation());
            Step=1;Until=FPlatformTime::Seconds()+.8f;return false;
        }
        if(Step==1)
        {
            if(Trip==0 && !Departure.IsValid()) Screenshot(TEXT("SafeTravelReadyDoor.png"));
            Departure=W;DestinationPackage=Room->TravelRoute()->Destination.GetLongPackageName();OldOpaque=NewOpaque=false;
            Player->AddMovementInput(FVector(0,-1,0),1.f);
            if(State->bWhiteTransition)
            {
                Test->TestTrue(TEXT("Entering door locks player input"),PC->IsGameplayLocked());
                Test->TestFalse(TEXT("Repeated overlap cannot start another transfer"),Door->Travel(Player));
                Step=2;return false;
            }
            return false;
        }
        if(Step==2 && Departure.Get()!=W && !State->bWhiteTransition)
        {
            Test->TestTrue(TEXT("Source map was covered by opaque white before loading"),OldOpaque);
            Test->TestTrue(TEXT("Destination remained covered while preparing the spawn"),NewOpaque);
            Test->TestEqual(TEXT("Loaded the route's actual destination package"),W->GetOutermost()->GetName(),DestinationPackage);
            ++Trip;Step=0;Until=FPlatformTime::Seconds()+.2f;return false;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSSafeTravelTest,"HorrorSystems.Progression.SafeWhiteTravel",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSSafeTravelTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FSafeTravelScenario(this));return true;}
#endif
