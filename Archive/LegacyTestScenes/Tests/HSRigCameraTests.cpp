#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSRoomActors.h"
#include "HSPlayerController.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    class FHSRigCameraScenario : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        int32 Stage=0;
        double Started=FPlatformTime::Seconds(), Until=Started+2;
        float InitialFOV=0, PelvisZ=0, UIZoom=0, StandingCameraZ=0;
        FVector Feet[2];
        float Lengths[4];
        TArray<TPair<TWeakObjectPtr<UPointLightComponent>,bool>> LightVisibility;
        void Wheel(AHSPlayerController* PC,FKey Key,int32 Count)
        {
            for(int32 I=0;I<Count;++I)
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1.f));
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0.f));
            }
        }
        void Next(float Seconds) { ++Stage; Until=FPlatformTime::Seconds()+Seconds; }
        FVector Bone(AHSCharacter* C,FName Name) { return C->GetMesh()->GetSocketLocation(Name); }
    public:
        explicit FHSRigCameraScenario(FAutomationTestBase* T):Test(T) {}
        virtual bool Update() override
        {
            if(FPlatformTime::Seconds()-Started>25) { Test->AddError(TEXT("Rig/camera scenario timed out")); return true; }
            if(FPlatformTime::Seconds()<Until) return false;
            UWorld* W=nullptr;
            for(const auto& Context:GEngine->GetWorldContexts())
                if(Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE) W=Context.World();
            auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
            auto* C=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
            if(!C) return false;
            if(Stage==0)
            {
                if(auto* Director=AHSRoomDirector::Find(W)) Director->FinishIntro();
                PC->SetGameplayLocked(false);
                C->UnCrouch(); C->ClearMovementModifiers();
                C->SetActorLocation(FVector(-1800,0,100));
                C->SetActorRotation(FRotator::ZeroRotator);
                PC->SetControlRotation(FRotator(-8,0,0));
                InitialFOV=C->Camera->FieldOfView;
                Test->TestTrue(TEXT("First-person camera attached to capsule"),C->Camera->GetAttachParent()==C->GetCapsuleComponent());
                Test->TestTrue(TEXT("First-person camera uses view rotation"),C->Camera->bUsePawnControlRotation);
                Test->TestTrue(TEXT("Character faces view yaw"),C->bUseControllerRotationYaw && !C->GetCharacterMovement()->bOrientRotationToMovement);
                Test->TestTrue(TEXT("Own head does not occlude first-person view"),C->GetMesh()->IsBoneHiddenByName("neck_01"));
                UPointLightComponent* Aura=nullptr;
                TArray<UPointLightComponent*> PointLights; C->GetComponents(PointLights);
                for(auto* Light:PointLights) if(Light->GetFName()==TEXT("PlayerAuraLight")) Aura=Light;
                Test->TestNotNull(TEXT("Player carries omnidirectional near light"),Aura);
                if(Aura)
                {
                    Test->TestTrue(TEXT("Player aura always visible"),Aura->IsVisible() && Aura->Intensity>0);
                    Test->TestTrue(TEXT("Aura lights only nearby space"),Aura->AttenuationRadius>=200 && Aura->AttenuationRadius<=500);
                    Test->TestTrue(TEXT("Aura follows player eyes"),Aura->GetAttachParent()==C->Camera);
                    Test->TestTrue(TEXT("Aura casts obstacle shadows"),Aura->CastShadows);
                }
                for(FName Name:{FName("upperarm_l"),FName("lowerarm_l"),FName("hand_l"),FName("upperarm_r"),FName("lowerarm_r"),FName("hand_r"),FName("thigh_l"),FName("calf_l"),FName("foot_l"),FName("thigh_r"),FName("calf_r"),FName("foot_r")})
                    Test->TestTrue(*FString::Printf(TEXT("Rig bone bound: %s"),*Name.ToString()),C->GetMesh()->GetBoneIndex(Name)!=INDEX_NONE);
                Wheel(PC,EKeys::MouseScrollUp,30); Next(.7f);
            }
            else if(Stage==1)
            {
                Test->TestTrue(TEXT("Wheel narrows first-person FOV"),C->Camera->FieldOfView<InitialFOV-10);
                Test->TestTrue(TEXT("FOV has safe near limit"),C->Camera->FieldOfView>=59.f);
                Wheel(PC,EKeys::MouseScrollDown,60); Next(.7f);
            }
            else if(Stage==2)
            {
                Test->TestTrue(TEXT("Wheel widens first-person FOV"),C->Camera->FieldOfView>InitialFOV+5);
                Test->TestTrue(TEXT("FOV has safe far limit"),C->Camera->FieldOfView<=96.f);
                UIZoom=C->Camera->FieldOfView;
                PC->ToggleHotbarMouse(); Wheel(PC,EKeys::MouseScrollUp,30);
                // InputKey queues events; allow a frame in UI mode before checking.
                Stage=22; Until=FPlatformTime::Seconds()+.3;
            }
            else if(Stage==22)
            {
                Test->TestTrue(TEXT("UI mode ignores camera wheel"),FMath::IsNearlyEqual(C->Camera->FieldOfView,UIZoom,.1f));
                PC->ToggleHotbarMouse();
                Test->TestTrue(TEXT("First-person can look down and up"),PC->PlayerCameraManager->ViewPitchMin<=-79 && PC->PlayerCameraManager->ViewPitchMax>=79);
                Wheel(PC,EKeys::MouseScrollUp,5);
                Stage=3; Until=FPlatformTime::Seconds()+.8;
            }
            else if(Stage==3)
            {
                PelvisZ=Bone(C,TEXT("pelvis")).Z;
                StandingCameraZ=C->Camera->GetComponentLocation().Z;
                Feet[0]=Bone(C,TEXT("foot_l")); Feet[1]=Bone(C,TEXT("foot_r"));
                Lengths[0]=FVector::Distance(Bone(C,"thigh_l"),Bone(C,"calf_l"));
                Lengths[1]=FVector::Distance(Bone(C,"calf_l"),Feet[0]);
                Lengths[2]=FVector::Distance(Bone(C,"thigh_r"),Bone(C,"calf_r"));
                Lengths[3]=FVector::Distance(Bone(C,"calf_r"),Feet[1]);
                Test->AddInfo(FString::Printf(TEXT("Standing pelvis %.2f feet %.2f / %.2f"),PelvisZ,Feet[0].Z,Feet[1].Z));
                PC->SetControlRotation(FRotator(-35,0,0));
                C->Crouch(); Next(.8f);
            }
            else if(Stage==4)
            {
                Test->TestTrue(TEXT("Crouch pelvis lowers"),Bone(C,"pelvis").Z<PelvisZ-25);
                for(int32 Side=0;Side<2;++Side)
                {
                    const FName Thigh=Side==0?"thigh_l":"thigh_r", Calf=Side==0?"calf_l":"calf_r", Foot=Side==0?"foot_l":"foot_r";
                    Test->TestTrue(TEXT("Crouch feet remain grounded"),FMath::Abs(Bone(C,Foot).Z-Feet[Side].Z)<4);
                    Test->TestTrue(TEXT("Thigh length preserved"),FMath::Abs(FVector::Distance(Bone(C,Thigh),Bone(C,Calf))-Lengths[Side*2])<.2);
                    Test->TestTrue(TEXT("Calf length preserved"),FMath::Abs(FVector::Distance(Bone(C,Calf),Bone(C,Foot))-Lengths[Side*2+1])<.2);
                }
                Test->AddInfo(FString::Printf(TEXT("Crouched pelvis %.2f feet %.2f / %.2f"),Bone(C,"pelvis").Z,Bone(C,"foot_l").Z,Bone(C,"foot_r").Z));
                Test->TestTrue(TEXT("Crouch lowers first-person eyes"),C->Camera->GetComponentLocation().Z<StandingCameraZ-30);
                Test->TestTrue(TEXT("Camera stays inside capsule horizontally"),C->Camera->GetRelativeLocation().Size2D()<C->GetCapsuleComponent()->GetScaledCapsuleRadius()-10);
                Test->TestTrue(TEXT("Crouch camera below capsule ceiling"),C->Camera->GetRelativeLocation().Z<C->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
                if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/FirstPersonCrouch.png")),true,false);
                }
                Next(.2f);
            }
            else if(Stage==5)
            {
                C->UnCrouch(); PC->SetControlRotation(FRotator(-30,180,0));
                for(TActorIterator<AActor> It(W);It;++It)
                {
                    TArray<UPointLightComponent*> Lights; It->GetComponents(Lights);
                    for(auto* Light:Lights) { LightVisibility.Emplace(Light,Light->IsVisible()); Light->SetVisibility(false); }
                }
                Next(.3f);
            }
            else if(Stage==6)
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/PlayerLightOff.png")),true,false);
                Next(.15f);
            }
            else if(Stage==7)
            {
                for(auto& Light:LightVisibility)
                    if(Light.Key.IsValid() && Light.Key->GetFName()==TEXT("PlayerAuraLight")) Light.Key->SetVisibility(true);
                Next(.3f);
            }
            else if(Stage==8)
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/PlayerLightOnly.png")),true,false);
                Next(.15f);
            }
            else
            {
                for(auto& Light:LightVisibility) if(Light.Key.IsValid()) Light.Key->SetVisibility(Light.Value);
                C->UnCrouch(); return true;
            }
            return false;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRigCameraTest,"HorrorSystems.Runtime.RigAndCamera",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSRigCameraTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FHSRigCameraScenario(this));
    return true;
}
#endif
