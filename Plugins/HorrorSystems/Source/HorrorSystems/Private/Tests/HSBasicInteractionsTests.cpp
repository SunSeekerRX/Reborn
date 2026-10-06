#include "Misc/AutomationTest.h"
#include "HSSceneInteractions.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSSettings.h"
#include "Materials/MaterialInterface.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FBasicInteractions : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int Stage=0;
    double Until=FPlatformTime::Seconds()+3;
    double FrozenTime=0;
    float Remaining=0;
    FVector BookStart,PaintingStart,OtherStart;
    bool bInspectionCaptured=false;
    TWeakObjectPtr<AHSInspectTrigger> Trigger;
    TWeakObjectPtr<AHSSwapPainting> First,Second;
    void Screenshot(const TCHAR* Name)
    {if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification"),Name),true,false);}
    void Focus(AHSCharacter* Player,AHSPlayerController* PC,AActor* Target)
    {
        FVector Center,Extent; Target->GetActorBounds(false,Center,Extent);
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetActorLocation(FVector(Center.X+170,Center.Y,198));
        PC->SetControlRotation((Center-Player->Camera->GetComponentLocation()).Rotation());
        PC->PlayerCameraManager->UpdateCamera(0);
        Player->RefreshInteractionFocus();
        FVector View;FRotator Rot;PC->GetPlayerViewPoint(View,Rot);FHitResult Hit;
        WarningsTrace(Player->GetWorld(),Player,View,Rot,Hit);
        Test->AddInfo(FString::Printf(TEXT("Focus target=%s actual=%s hit=%s player=%s view=%s targetcenter=%s"),*Target->GetName(),*GetNameSafe(Player->FocusedInteraction),*GetNameSafe(Hit.GetActor()),*Player->GetActorLocation().ToString(),*View.ToString(),*Center.ToString()));
    }
    void WarningsTrace(UWorld* W,AHSCharacter* Player,FVector View,FRotator Rot,FHitResult& Hit)
    {FCollisionQueryParams Params(SCENE_QUERY_STAT(BasicFocusEvidence),false,Player);W->SweepSingleByChannel(Hit,View,View+Rot.Vector()*360,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(18),Params);}
    void CheckSingleHighlight(UWorld* W,AHSCharacter* Player)
    {
        int Count=0;
        for(TActorIterator<AActor> It(W);It;++It)
            if(auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>(); Mesh && Mesh->GetOverlayMaterial()) ++Count;
        Test->TestTrue(TEXT("At most one world interaction highlight"),Count<=1);
        Test->TestNotNull(TEXT("Current focused target has highlight"),Player->GetHighlightedMesh());
    }
public:
    explicit FBasicInteractions(FAutomationTestBase* InTest):Test(InTest){}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()<Until) return false;
        UWorld* W=nullptr;for(auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        auto* Room=AHSRoomDirector::Find(W);
        if(!Player || !Room) {Test->AddError(TEXT("Run on a configured Basic map"));return true;}
        auto* Progress=Room->Progress();
        switch(Stage++)
        {
        case 0:
            if(Room->WindowSequence && !Room->WindowSequence->bPlaying) Room->WindowSequence->Play();
            if(Room->WindowSequence && Room->WindowSequence->Performer)
            {
                Room->WindowSequence->Tick(FMath::Max(0.f,Room->WindowSequence->Duration*.62f-Room->WindowSequence->Elapsed));
                const FVector Cam=Room->WindowSequence->Camera->GetComponentLocation();FHitResult ViewHit;FCollisionQueryParams Params(SCENE_QUERY_STAT(BasicWindowEvidence),true);
                W->LineTraceSingleByChannel(ViewHit,Cam,Room->WindowSequence->Performer->GetActorLocation()+FVector(0,0,75),ECC_Visibility,Params);
                Test->AddInfo(FString::Printf(TEXT("Window camera=%s target=%s sightHit=%s"),*Cam.ToString(),*Room->WindowSequence->Performer->GetActorLocation().ToString(),*GetNameSafe(ViewHit.GetActor())));
                Screenshot(TEXT("BasicCloseWindow.png")); Until=FPlatformTime::Seconds()+.3;return false;
            }
            // If intro was previously seen, continue with the gameplay checks.
            --Stage; Stage=1;
            [[fallthrough]];
        case 1:
            Room->FinishIntro();PC->SetGameplayLocked(false);
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;It->GetCharacterMovement()->StopMovementImmediately();It->SetActorLocation(FVector(-800,-900,192));}
            for(TActorIterator<AHSInspectTrigger> It(W);It;++It) if(It->GetActorLocation().Y>-1200) {Trigger=*It;break;}
            for(TActorIterator<AHSSwapPainting> It(W);It;++It) if(It->GetActorLocation().Y>-1200) {if(!First.IsValid()) First=*It;else if(!Second.IsValid()) Second=*It;}
            if(!Trigger.IsValid() || !First.IsValid() || !Second.IsValid()) {Test->AddError(TEXT("Basic furniture interaction bindings missing"));return true;}
            Focus(Player,PC,Trigger.Get());
            Test->TestEqual(TEXT("Inspectable record is chosen by camera trace"),Player->FocusedInteraction.Get(),static_cast<AActor*>(Trigger.Get()));
            CheckSingleHighlight(W,Player); BookStart=Trigger->Obstacle->GetActorLocation();
            Player->Interact();
            Test->TestTrue(TEXT("E opens 3D inspection"),PC->IsInspecting());
            Test->TestTrue(TEXT("Inspection pauses the game"),UGameplayStatics::IsGamePaused(W));
            FrozenTime=W->GetTimeSeconds();Remaining=Progress->GetRemaining();
            Until=FPlatformTime::Seconds()+2;return false;
        case 2:
            if(!bInspectionCaptured) {bInspectionCaptured=true;Screenshot(TEXT("BasicInspection.png"));--Stage;Until=FPlatformTime::Seconds()+.3;return false;}
            Test->TestEqual(TEXT("World clock stays frozen while inspecting"),W->GetTimeSeconds(),FrozenTime);
            Test->TestEqual(TEXT("Room countdown stays frozen while inspecting"),Progress->GetRemaining(),Remaining);
            Test->TestTrue(TEXT("Bookshelf has not fallen during inspection"),Trigger->Obstacle->GetActorLocation().Equals(BookStart));
            PC->CloseInspection();
            Test->TestFalse(TEXT("Closing resumes time"),UGameplayStatics::IsGamePaused(W));
            Test->TestTrue(TEXT("Closing starts bookshelf fall"),Trigger->Obstacle->bFalling);
            Player->SetActorLocation(FVector(-550,Trigger->Obstacle->GetActorLocation().Y+160,198));
            Until=FPlatformTime::Seconds()+1.6;return false;
        case 3:
        {
            auto* Book=Trigger->Obstacle.Get();
            Test->TestTrue(TEXT("Bookshelf finishes falling"),Book->bCollapsed);
            Test->TestEqual(TEXT("Fallen bookshelf retains solid collision"),Book->Mesh->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics);
            FVector Center,Extent;Book->GetActorBounds(false,Center,Extent);
            Test->TestTrue(TEXT("Bookshelf is horizontal on the floor"),Extent.X>Extent.Z && FMath::IsNearlyEqual(Center.Z-Extent.Z,100.f,3.f));
            FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(BasicFallenShelf),false,Player);
            W->SweepSingleByChannel(Hit,FVector(Center.X+Extent.X+100,Center.Y+160,198),FVector(Center.X-Extent.X-100,Center.Y+160,198),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Params);
            Test->TestTrue(TEXT("Actual capsule sweep hits the fallen bookshelf"),Hit.GetActor()==Book);
            Test->AddInfo(FString::Printf(TEXT("Book sweep hit=%s center=%s extent=%s profile=%s"),*GetNameSafe(Hit.GetActor()),*Center.ToString(),*Extent.ToString(),*Book->Mesh->GetCollisionProfileName().ToString()));
            PC->SetControlRotation((Center-Player->Camera->GetComponentLocation()).Rotation());Screenshot(TEXT("BasicFallenBookshelf.png"));
            Until=FPlatformTime::Seconds()+.3;return false;
        }
        case 4:
            Focus(Player,PC,First.Get());CheckSingleHighlight(W,Player);
            Test->TestEqual(TEXT("Painting can be selected from crosshair"),Player->FocusedInteraction.Get(),static_cast<AActor*>(First.Get()));
            PaintingStart=First->GetActorLocation();OtherStart=Second->GetActorLocation();
            Player->Interact();
            Test->TestTrue(TEXT("First E remembers the first painting"),Player->SelectedPainting.Get()==First.Get());
            Test->TestFalse(TEXT("First E does not move painting"),First->bSwapping);
            Test->TestTrue(TEXT("Painting has blue highlight"),First->Mesh->GetOverlayMaterial() && First->Mesh->GetOverlayMaterial()->GetName()==TEXT("M_HighlightPainting"));
            Screenshot(TEXT("BasicPaintingHighlight.png"));Until=FPlatformTime::Seconds()+.3;return false;
        case 5:
            Focus(Player,PC,Second.Get());CheckSingleHighlight(W,Player);
            Test->TestNull(TEXT("First highlight clears when second is focused"),First->Mesh->GetOverlayMaterial());
            Player->Interact();
            Test->TestTrue(TEXT("Second E starts both swap animations"),First->bSwapping && Second->bSwapping);
            Until=FPlatformTime::Seconds()+1.2;return false;
        case 6:
            Test->TestTrue(TEXT("First painting occupies second position"),First->GetActorLocation().Equals(OtherStart,.1f));
            Test->TestTrue(TEXT("Second painting occupies first position"),Second->GetActorLocation().Equals(PaintingStart,.1f));
            Test->TestFalse(TEXT("Both animations finish"),First->bSwapping || Second->bSwapping);
            Test->TestFalse(TEXT("Swap selection clears after completion"),Player->SelectedPainting.IsValid());
            Screenshot(TEXT("BasicPaintingsSwapped.png"));
            Test->TestTrue(TEXT("Unavailable collapse and painting sounds remain unassigned"),GetDefault<UHSSettings>()->CollapseSound.IsNull() && GetDefault<UHSSettings>()->PaintingSwapSound.IsNull());
            return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicInteractionsTest,"HorrorSystems.Basic.Interactions",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FBasicInteractionsTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FBasicInteractions(this));return true;}
#endif
