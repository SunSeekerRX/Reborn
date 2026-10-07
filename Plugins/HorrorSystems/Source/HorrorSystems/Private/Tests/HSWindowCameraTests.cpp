#include "Misc/AutomationTest.h"
#include "HSRoomActors.h"
#include "HSCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "HSPlayerController.h"
#include "HSWorldActors.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
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
    bool WalkingFacingChecked=false,LookingChecked=false,RunningChecked=false;
    float ApproachSpeed=0;
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
            // The current story starts this performance after reading B's clue in the main room.
            bool FoundStandingPosition=false;
            auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
            for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("Warning_B"))
            {
                FCollisionQueryParams Q;Q.AddIgnoredActor(Player);Q.AddIgnoredActor(*It);
                for(int32 I=0;Nav && I<16 && !FoundStandingPosition;++I)
                {
                    const float Angle=I*2.f*PI/16.f;FNavLocation Floor;
                    const FVector Candidate=It->GetActorLocation()+FVector(FMath::Cos(Angle)*170.f,FMath::Sin(Angle)*170.f,-100.f);
                    if(!Nav->ProjectPointToNavigation(Candidate,Floor,FVector(40,40,250))) continue;
                    const FVector Standing=Floor.Location+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
                    if(W->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q)) continue;
                    Player->SetActorLocation(Standing);FoundStandingPosition=true;
                }
                break;
            }
            if(!FoundStandingPosition) {Test->AddError(TEXT("No unobstructed standing floor near B's clue"));return true;}
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
        const float Phase=Sequence->Elapsed/FMath::Max(1.f,Sequence->Duration);
        if(Sequence->Performer)
        {
            auto* Ghost=Sequence->Performer.Get();
            if(!WalkingFacingChecked && Phase>.5f && Phase<.58f)
            {
                WalkingFacingChecked=true;ApproachSpeed=Ghost->GetVelocity().Size2D();
                const float Alignment=FVector::DotProduct(Ghost->GetActorForwardVector(),Ghost->GetVelocity().GetSafeNormal2D());
                Test->AddInfo(FString::Printf(TEXT("Window walk orientation: alignment %.3f, yaw %.1f, velocity %s"),Alignment,Ghost->GetActorRotation().Yaw,*Ghost->GetVelocity().ToString()));
                Test->TestTrue(TEXT("Passing monster faces its walking direction"),Alignment>.9f);
            }
            if(!LookingChecked && Phase>.68f && Phase<.70f)
            {LookingChecked=true;Test->TestTrue(TEXT("Window monster turns toward the window"),FVector::DotProduct(Ghost->GetActorForwardVector(),(Current-Ghost->GetActorLocation()).GetSafeNormal2D())>.9f);}
            if(!RunningChecked && Phase>.8f && Phase<.88f)
            {RunningChecked=true;Test->TestTrue(TEXT("Monster runs away faster than it approached"),Ghost->GetVelocity().Size2D()>ApproachSpeed*1.5f);}
        }
        if(!WindowChecked && Sequence->Elapsed>Sequence->Duration*.60f)
        {
            WindowChecked=true;
            const FVector Destination=Sequence->GetActorTransform().TransformPosition(Sequence->CameraOffset);
            Test->TestTrue(TEXT("Camera reaches the window rather than stopping at a wall"),Current.Equals(Destination,2.f));
            Test->AddInfo(FString::Printf(TEXT("Window camera reached %s (target %s)"),*Current.ToString(),*Destination.ToString()));
            Test->TestNotNull(TEXT("Window monster performer exists"),Sequence->Performer.Get());
            Test->TestTrue(TEXT("Cinematic fill illuminates the window performer"),Sequence->WindowFill->IsVisible());
            if(Sequence->Performer)
            {
                Test->AddInfo(FString::Printf(TEXT("Window performer mesh bounds: center %s extent %s"),*Sequence->Performer->GetMesh()->Bounds.Origin.ToString(),*Sequence->Performer->GetMesh()->Bounds.BoxExtent.ToString()));
                FHitResult Sight;
                W->LineTraceSingleByChannel(Sight,Current,Sequence->Performer->GetActorLocation()+FVector(0,0,65),ECC_Visibility,Params);
                if(Sight.bBlockingHit) Test->AddInfo(FString::Printf(TEXT("Sight blocked by %s at %s"),*GetNameSafe(Sight.GetActor()),*Sight.ImpactPoint.ToString()));
                Test->TestFalse(TEXT("Window monster is not occluded by room furniture or walls"),Sight.bBlockingHit);
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/RoomAWindowFixed.png")),true,false);
        }
        if(!Sequence->bPlaying)
        {
            Test->TestFalse(TEXT("Cinematic state cleared"),Room->Progress()->bCinematic);
            Test->TestFalse(TEXT("Cinematic fill stops with the performance"),Sequence->WindowFill->IsVisible());
            Test->TestFalse(TEXT("Player control restored"),PC->IsGameplayLocked());
            Test->TestTrue(TEXT("Camera returns to the player"),PC->GetViewTarget()==Player);
            Test->TestTrue(TEXT("Entire cinematic was sampled"),Samples>30);
            Test->TestTrue(TEXT("Walk, look and run phases were sampled"),WalkingFacingChecked&&LookingChecked&&RunningChecked);
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
