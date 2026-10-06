#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSSceneInteractions.h"
#include "HSProgression.h"
#include "HSSettings.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "AudioDevice.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FRoomAMovement : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Until=0;
    double Started=FPlatformTime::Seconds();
    FVector Before;
    void Key(AHSPlayerController* PC,FKey K,EInputEvent E) {PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f));}
    void Next(float Wait) {++Step;Until=FPlatformTime::Seconds()+Wait;}
    void Reset(AHSCharacter* P) {P->ClearMovementModifiers();P->SetActorLocation(FVector(-700,300,194));}
public:
    explicit FRoomAMovement(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>25) {Test->AddError(TEXT("RoomA movement timed out"));return true;}
        if(FPlatformTime::Seconds()<Until) return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* P=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        auto* Room=AHSRoomDirector::Find(W);if(!P || !Room) {Test->AddError(TEXT("Run on RoomA"));return true;}
        const auto* S=GetDefault<UHSSettings>();
        switch(Step)
        {
        case 0:
            Room->FinishIntro();PC->SetGameplayLocked(false);P->ClearMovementModifiers();
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCinematicActor=true;It->bCanDamagePlayer=false;if(It->GetController()) It->GetController()->StopMovement();}
            PC->SetControlRotation(FRotator(0,90,0));Reset(P);Next(.2f);break;
        case 1: Key(PC,EKeys::W,IE_Pressed);Before=P->GetActorLocation();Next(.4f);break;
        case 2:
            Test->TestTrue(TEXT("W walks forward at normal speed"),P->GetActorLocation().Y>Before.Y+100 && FMath::IsNearlyEqual(P->GetVelocity().Size2D(),S->WalkSpeed,5));
            Key(PC,EKeys::W,IE_Released);Reset(P);Key(PC,EKeys::W,IE_Pressed);Key(PC,EKeys::D,IE_Pressed);Next(.35f);break;
        case 3:
            Test->TestTrue(TEXT("Diagonal input stays at normal speed"),FMath::IsNearlyEqual(P->GetVelocity().Size2D(),S->WalkSpeed,5));
            Key(PC,EKeys::W,IE_Released);Key(PC,EKeys::D,IE_Released);Reset(P);Key(PC,EKeys::W,IE_Pressed);Key(PC,EKeys::LeftShift,IE_Pressed);Next(.35f);break;
        case 4:
            Test->TestTrue(TEXT("Shift reaches sprint speed"),FMath::IsNearlyEqual(P->GetVelocity().Size2D(),S->SprintSpeed,5));
            Key(PC,EKeys::W,IE_Released);Key(PC,EKeys::LeftShift,IE_Released);Reset(P);Key(PC,EKeys::W,IE_Pressed);Key(PC,EKeys::LeftControl,IE_Pressed);Next(.6f);break;
        case 5:
            Test->TestTrue(TEXT("Held Ctrl walks slowly without crouching"),P->IsSlowWalking() && !P->bIsCrouched && FMath::IsNearlyEqual(P->GetVelocity().Size2D(),S->SlowSpeed,5));
            Key(PC,EKeys::W,IE_Released);Key(PC,EKeys::LeftControl,IE_Released);Reset(P);Key(PC,EKeys::LeftControl,IE_Pressed);Key(PC,EKeys::LeftControl,IE_Released);Next(.25f);break;
        case 6: Test->TestTrue(TEXT("Tapped Ctrl crouches"),P->bIsCrouched);Key(PC,EKeys::W,IE_Pressed);Key(PC,EKeys::LeftShift,IE_Pressed);Next(.4f);break;
        case 7:
            Test->TestTrue(TEXT("Crouching cannot sprint"),P->bIsCrouched && FMath::IsNearlyEqual(P->GetVelocity().Size2D(),S->CrouchSpeed,5));
            Test->TestTrue(TEXT("First-person eye remains above the floor while crouching"),P->Camera->GetComponentLocation().Z>170);
            Key(PC,EKeys::W,IE_Released);Key(PC,EKeys::LeftShift,IE_Released);Key(PC,EKeys::LeftControl,IE_Pressed);Key(PC,EKeys::LeftControl,IE_Released);Next(.3f);break;
        case 8:
            Test->TestFalse(TEXT("Second Ctrl tap stands back up"),P->bIsCrouched);Reset(P);Before=P->GetActorLocation();PC->SetGameplayLocked(true);Key(PC,EKeys::W,IE_Pressed);Next(.3f);break;
        case 9:
            Test->TestTrue(TEXT("Locked input does not move player"),FVector::Dist2D(P->GetActorLocation(),Before)<2);
            Key(PC,EKeys::W,IE_Released);PC->SetGameplayLocked(false);
            for(TActorIterator<AHSPortal> It(W);It;++It)
            {
                Room->Progress()->CollectClue(TEXT("Key_1"));
                Test->TestFalse(TEXT("RoomA preview disables inter-room travel"),It->bTravelEnabled);
                Test->TestTrue(TEXT("Preview exit stays locked even with the clue"),It->IsLocked());
                Test->TestFalse(TEXT("Travel cannot load unfinished RoomB"),It->Travel(P));
            }
            if(!FParse::Param(FCommandLine::Get(),TEXT("nosound")))
            {
                Test->TestTrue(TEXT("Real audio device is available"),W->GetAudioDevice().IsValid());
                for(TActorIterator<AHSSceneAudio> It(W);It;++It) Test->TestTrue(TEXT("RoomA background audio is playing"),It->Ambient->IsPlaying() && It->Ambient->VolumeMultiplier>0);
            }
            return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRoomAMovementTest,"HorrorSystems.Basic.RoomAMovement",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSRoomAMovementTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FRoomAMovement(this));return true;}
#endif
