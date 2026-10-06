#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSSettings.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/AudioComponent.h"
#include "InputKeyEventArgs.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FHSMovementEdges : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Started=FPlatformTime::Seconds(),Until=Started+2;
    float StraightSpeed=0;
    FVector Position;
    TWeakObjectPtr<AHSMonster> Monster;
    void Key(AHSPlayerController* PC,FKey K,EInputEvent E) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f)); }
    void Next(float Seconds) { ++Step; Until=FPlatformTime::Seconds()+Seconds; }
public:
    explicit FHSMovementEdges(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds(); if(Now-Started>30) { Test->AddError(TEXT("Movement edge scenario timeout")); return true; }
        if(Now<Until) return false;
        UWorld* W=nullptr; for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE) W=Context.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr; auto* C=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        auto* D=AHSRoomDirector::Find(W); if(!C || !D) return false;
        const auto* S=GetDefault<UHSSettings>();
        switch(Step)
        {
        case 0:
            D->FinishIntro(); PC->CloseInspection(); PC->SetGameplayLocked(false); C->UnCrouch(); C->ClearMovementModifiers();
            C->SetActorLocation(FVector(-700,-800,100)); PC->SetControlRotation(FRotator(0,0,0)); Key(PC,EKeys::W,IE_Pressed); Next(.65f); break;
        case 1:
            StraightSpeed=C->GetVelocity().Size2D(); Test->TestTrue(TEXT("Normal movement reaches configured speed"),FMath::IsNearlyEqual(StraightSpeed,S->WalkSpeed,3));
            Key(PC,EKeys::D,IE_Pressed); Next(.6f); break;
        case 2:
            Test->TestTrue(TEXT("Diagonal movement does not gain speed"),FMath::IsNearlyEqual(C->GetVelocity().Size2D(),StraightSpeed,3));
            Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::D,IE_Released); C->ClearMovementModifiers(); C->SetActorLocation(FVector(-700,-800,100)); Position=C->GetActorLocation(); Key(PC,EKeys::D,IE_Pressed); Next(.6f); break;
        case 3:
            Key(PC,EKeys::D,IE_Released); Test->TestTrue(TEXT("Strafe moves right in camera plane"),C->GetActorLocation().Y>Position.Y+120);
            C->ClearMovementModifiers(); Position=C->GetActorLocation(); Key(PC,EKeys::S,IE_Pressed); Next(.6f); break;
        case 4:
            Key(PC,EKeys::S,IE_Released); Test->TestTrue(TEXT("Backward movement follows camera plane"),C->GetActorLocation().X<Position.X-120);
            C->ClearMovementModifiers(); C->SetActorLocation(FVector(2400,1000,100)); Key(PC,EKeys::W,IE_Pressed); Key(PC,EKeys::LeftShift,IE_Pressed); Next(.8f); break;
        case 5:
            Key(PC,EKeys::W,IE_Released); Key(PC,EKeys::LeftShift,IE_Released); Test->TestTrue(TEXT("Sprint cannot pass room wall"),C->GetActorLocation().X<2530);
            Test->TestTrue(TEXT("Character stays grounded"),!C->GetCharacterMovement()->IsFalling());
            C->ClearMovementModifiers(); C->SetActorLocation(FVector(-700,-800,100)); Position=C->GetActorLocation();
            PC->SetGameplayLocked(true); Key(PC,EKeys::W,IE_Pressed); Next(.5f); break;
        case 6:
            Key(PC,EKeys::W,IE_Released); Test->TestTrue(TEXT("Locked input cannot move character"),FVector::Dist2D(C->GetActorLocation(),Position)<2);
            PC->SetGameplayLocked(false); D->Progress()->BeginCountdown();
            for(TActorIterator<AHSMonster> It(W);It;++It) if(!It->bCinematicActor) { Monster=*It; break; }
            if(!Monster.IsValid()) { Test->AddError(TEXT("No pursuing monster")); return true; }
            Monster->SetActorLocation(FVector(1000,1000,100)); Monster->SetMovementState(EHSMovementState::Slow); Next(1); break;
        case 7:
            Test->TestTrue(TEXT("Monster slow state speed"),FMath::IsNearlyEqual(Monster->GetCharacterMovement()->MaxWalkSpeed,S->NearSpeed,1));
            Monster->SetMovementState(EHSMovementState::Normal); Next(1); break;
        case 8:
            Test->TestTrue(TEXT("Monster normal state speed"),FMath::IsNearlyEqual(Monster->GetCharacterMovement()->MaxWalkSpeed,S->NormalSpeed,1));
            Monster->SetMovementState(EHSMovementState::Fast); Next(1); break;
        case 9:
            Test->TestTrue(TEXT("Monster fast state speed"),FMath::IsNearlyEqual(Monster->GetCharacterMovement()->MaxWalkSpeed,S->FarSpeed,1));
            Test->TestNotNull(TEXT("Monster footstep assigned"),Monster->FootstepSound.Get()); Test->TestTrue(TEXT("Monster presence spatialized"),Monster->PresenceAudio->Sound!=nullptr && Monster->PresenceAudio->bAllowSpatialization && Monster->PresenceAudio->bOverrideAttenuation);
            Monster->SetMovementState(EHSMovementState::Normal,true); return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSMovementEdgesTest,"HorrorSystems.Runtime.MovementEdges",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSMovementEdgesTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FHSMovementEdges(this)); return true; }
#endif
