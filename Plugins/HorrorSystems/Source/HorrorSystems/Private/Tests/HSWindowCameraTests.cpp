#include "Misc/AutomationTest.h"
#include "HSRoomActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FWindowCameraCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHSWindowSequence> Sequence;
    FVector Previous;
    bool Started=false;
    bool HitReported=false;
    int32 Samples=0;
    double StartedAt=0;
    bool WindowChecked=false;
public:
    explicit FWindowCameraCheck(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        auto* Room=AHSRoomDirector::Find(W);
        if(!Player || !Room || !Room->WindowSequence) {Test->AddError(TEXT("Configured RoomA is required"));return true;}
        if(!Started)
        {
            Sequence=Room->WindowSequence;Sequence->Finish();PC->PlayerCameraManager->UpdateCamera(0);Sequence->Play();
            Previous=Sequence->Camera->GetComponentLocation();Started=true;StartedAt=FPlatformTime::Seconds();
            Test->AddInfo(FString::Printf(TEXT("Intro begins at %s; destination %s"),*Previous.ToString(),*Sequence->GetActorTransform().TransformPosition(Sequence->CameraOffset).ToString()));
        }
        const FVector Current=Sequence->Camera->GetComponentLocation();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(WindowCameraClearance),true,Player);
        Params.AddIgnoredActor(Sequence.Get());
        for(TActorIterator<AHSMonster> It(W);It;++It) Params.AddIgnoredActor(*It);
        FHitResult Hit;
        if(!HitReported && W->SweepSingleByChannel(Hit,Previous,Current,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Params))
        {
            HitReported=true;
            Test->AddError(FString::Printf(TEXT("Opening camera crosses geometry: t=%.3f from=%s to=%s blocker=%s point=%s penetration=%d"),Sequence->Elapsed,*Previous.ToString(),*Current.ToString(),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),Hit.bStartPenetrating));
        }
        Previous=Current;++Samples;
        if(!WindowChecked && Sequence->Elapsed>Sequence->Duration*.60f)
        {
            WindowChecked=true;
            const FVector Destination=Sequence->GetActorTransform().TransformPosition(Sequence->CameraOffset);
            Test->TestTrue(TEXT("Camera reaches the window rather than stopping at a wall"),Current.Equals(Destination,2.f));
            Test->TestNotNull(TEXT("Window monster performer exists"),Sequence->Performer.Get());
            if(Sequence->Performer)
            {
                FHitResult Sight;
                W->LineTraceSingleByChannel(Sight,Current,Sequence->Performer->GetActorLocation()+FVector(0,0,65),ECC_Visibility,Params);
                Test->TestFalse(TEXT("Window monster is not occluded by room furniture or walls"),Sight.bBlockingHit);
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/RoomAWindowFixed.png")),true,false);
        }
        if(!Sequence->bPlaying)
        {
            Test->TestFalse(TEXT("Cinematic state cleared"),Room->Progress()->bCinematic);
            Test->TestFalse(TEXT("Player control restored"),PC->IsGameplayLocked());
            Test->TestTrue(TEXT("Camera returns to the player"),PC->GetViewTarget()==Player);
            Test->TestTrue(TEXT("Entire cinematic was sampled"),Samples>30);
            Test->AddInfo(FString::Printf(TEXT("Camera path checked over %d frames"),Samples));return true;
        }
        if(FPlatformTime::Seconds()-StartedAt>30) {Sequence->Finish();Test->AddError(TEXT("Intro did not finish"));return true;}
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSWindowCameraTest,"HorrorSystems.Basic.WindowCamera",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSWindowCameraTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FWindowCameraCheck(this));return true;}
#endif
