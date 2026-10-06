#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSWorldActors.h"
#include "HSAI.h"
#include "HSSettings.h"
#include "HSRoomActors.h"
#include "HSProgression.h"
#include "HSInspectionView.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Layout/Children.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UWorld* GameWorld()
    {
        for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE) return Context.World();
        return nullptr;
    }
    class FHSRuntimeScenario : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        int32 Stage=0;
        double Started=FPlatformTime::Seconds(), Until=Started+3;
        TWeakObjectPtr<UHSWorldState> Session;
        TWeakObjectPtr<AHSMonster> Monster;
        FVector OriginalLocation, MonsterLocation;
        double FrozenWorldTime=0;
        float OriginalMonsterDistance=0;
        FString CollectedKey;
        float HandDistance=0,UpperArmLength=0,ForearmLength=0;
        double ChaseStartedGameTime=0;
        bool DragThroughSlate(UWorld* W)
        {
            auto Window=W->GetGameViewport()?W->GetGameViewport()->GetWindow():nullptr;
            if(!Window.IsValid()) return false;
            TArray<TSharedRef<SWidget>> Slots;
            TFunction<void(TSharedRef<SWidget>)> Visit=[&](TSharedRef<SWidget> Widget)
            {
                if(Widget->GetTypeAsString()==TEXT("SHSSlot")) Slots.Add(Widget);
                FChildren* Children=Widget->GetChildren();
                for(int32 I=0; I<Children->Num(); ++I) Visit(Children->GetChildAt(I));
            };
            Visit(Window.ToSharedRef());
            if(Slots.Num()!=10) { Test->AddInfo(FString::Printf(TEXT("Slate slot discovery count=%d"),Slots.Num())); return false; }
            auto Center=[](TSharedRef<SWidget> Widget){ const auto& G=Widget->GetCachedGeometry(); return G.LocalToAbsolute(G.GetLocalSize()*.5f); };
            auto& Slate=FSlateApplication::Get();
            const FVector2D From=Center(Slots[0]), To=Center(Slots[5]);
            TSet<FKey> None, Pressed; Pressed.Add(EKeys::LeftMouseButton);
            Slate.SetCursorPos(From);
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,From,From,None,EKeys::Invalid,0,FModifierKeysState()),false);
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),FPointerEvent(0,From,From,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState()));
            const FVector2D Step=From+FVector2D(25,-15);
            Slate.SetCursorPos(Step);
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,Step,From,Pressed,EKeys::Invalid,0,FModifierKeysState()),false);
            Slate.SetCursorPos(To);
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,To,Step,Pressed,EKeys::Invalid,0,FModifierKeysState()),false);
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,To,To,None,EKeys::LeftMouseButton,0,FModifierKeysState()));
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,To,To,None,EKeys::Invalid,0,FModifierKeysState()),false);
            return Session->SelectedSlot==5 && Session->Slots[5].Item && !Session->Slots[0].Item;
        }
        void Screenshot(const TCHAR* Name)
        {
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification"),Name),true,false);
        }
        bool ManipulateInspectionThroughSlate(UWorld* W)
        {
            auto Window=W->GetGameViewport()->GetWindow();
            TSharedPtr<SHSInspectionView> View;
            TFunction<void(TSharedRef<SWidget>)> Visit=[&](TSharedRef<SWidget> Widget) {
                if(Widget->GetTypeAsString()==TEXT("SHSInspectionView")) View=StaticCastSharedRef<SHSInspectionView>(Widget);
                auto* Children=Widget->GetChildren();
                for(int32 I=0;I<Children->Num();++I) Visit(Children->GetChildAt(I));
            };
            if(!Window.IsValid()) return false;
            Visit(Window.ToSharedRef()); if(!View.IsValid()) return false;
            TArray<FFloat16Color> Pixels;
            auto* Target=View->GetRenderTarget();
            if(Target && Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels) && Pixels.Num()==Target->SizeX*Target->SizeY)
            {
                Test->TestTrue(TEXT("Preview background has inverse alpha one (transparent)"),Pixels[0].A.GetFloat()>.99f);
                Test->TestTrue(TEXT("Preview object has inverse alpha zero (opaque)"),Pixels[(Target->SizeY/2)*Target->SizeX+Target->SizeX/2].A.GetFloat()<.05f);
                const auto& CenterPixel=Pixels[(Target->SizeY/2)*Target->SizeX+Target->SizeX/2];
                Test->AddInfo(FString::Printf(TEXT("Preview HDR center: R=%.6f G=%.6f B=%.6f A=%.6f"),CenterPixel.R.GetFloat(),CenterPixel.G.GetFloat(),CenterPixel.B.GetFloat(),CenterPixel.A.GetFloat()));
            }
            else Test->AddError(TEXT("Could not read inspection transparency pixels"));
            auto& Slate=FSlateApplication::Get();
            const auto& G=View->GetCachedGeometry();
            const FVector2D From=G.LocalToAbsolute(G.GetLocalSize()*.5f),To=From+FVector2D(100,45);
            const FQuat Before=View->GetModelRotation();
            TSet<FKey> None,Pressed; Pressed.Add(EKeys::LeftMouseButton);
            Slate.SetCursorPos(From);
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,From,From,None,EKeys::Invalid,0,FModifierKeysState()),false);
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),FPointerEvent(0,From,From,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState()));
            Slate.SetCursorPos(To);
            Slate.ProcessMouseMoveEvent(FPointerEvent(0,To,From,Pressed,EKeys::Invalid,0,FModifierKeysState()),false);
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,To,To,None,EKeys::LeftMouseButton,0,FModifierKeysState()));
            const float BeforeZoom=View->GetZoom();
            Slate.ProcessMouseWheelOrGestureEvent(FPointerEvent(0,To,To,None,EKeys::Invalid,2.f,FModifierKeysState()),nullptr);
            Test->TestTrue(TEXT("Actual Slate mouse drag rotates inspection mesh"),!View->GetModelRotation().Equals(Before));
            Test->TestTrue(TEXT("Actual Slate mouse wheel zooms inspection mesh"),View->GetZoom()>BeforeZoom);
            return true;
        }
        void Key(AHSPlayerController* PC,FKey K,EInputEvent E) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f)); }
        void Next(float Wait=0) { ++Stage; Until=FPlatformTime::Seconds()+Wait; }
    public:
        explicit FHSRuntimeScenario(FAutomationTestBase* T):Test(T) {}
        virtual bool Update() override
        {
            const double Now=FPlatformTime::Seconds();
            if(Now-Started>70) { Test->AddError(TEXT("Runtime scenario timed out")); return true; }
            if(Now<Until) return false;
            UWorld* W=GameWorld();
            auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
            auto* C=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
            if(!PC || !C) return false;
            if(Stage==0)
            {
                if(UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Test_A")) { UGameplayStatics::OpenLevel(W,TEXT("/HorrorSystems/Maps/Test_A")); Until=Now+3; return false; }
                if(auto* Director=AHSRoomDirector::Find(W)) Director->FinishIntro();
                PC->SetGameplayLocked(false);
                Session=PC->GetSession(); Session->ResetSession();
                auto* Progress=W->GetGameInstance()->GetSubsystem<UHSProgression>();
                Progress->EnterRoom(TEXT("RoomA"),60); Progress->BeginCountdown();
                Test->TestNotNull(TEXT("Template mannequin loaded"),C->GetMesh()->GetSkeletalMeshAsset());
                Test->TestNotNull(TEXT("Native animation loaded"),C->GetMesh()->GetAnimInstance());
                bool Vision=false;
                for(TActorIterator<AHSVisionRig> It(W);It;++It) Vision=It->PostProcess->Settings.WeightedBlendables.Array.Num()>0;
                Test->TestTrue(TEXT("Depth fade postprocess applied"),Vision);
                auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
                FNavLocation Projected;
                Test->TestTrue(TEXT("NavMesh covers player start"),Nav && Nav->ProjectPointToNavigation(C->GetActorLocation(),Projected));
                Screenshot(TEXT("Gameplay.png"));
                OriginalLocation=C->GetActorLocation(); Key(PC,EKeys::W,IE_Pressed); Next(.7f);
            }
            else if(Stage==1)
            {
                Key(PC,EKeys::W,IE_Released);
                Test->TestTrue(TEXT("W advances character"),C->GetActorLocation().X>OriginalLocation.X+80);
                Key(PC,EKeys::LeftShift,IE_Pressed); Next(.1f);
            }
            else if(Stage==2)
            {
                Test->TestEqual(TEXT("Sprint speed"),C->GetCharacterMovement()->MaxWalkSpeed,GetDefault<UHSSettings>()->SprintSpeed);
                Key(PC,EKeys::LeftShift,IE_Released); Key(PC,EKeys::LeftControl,IE_Pressed); Next(.35f);
            }
            else if(Stage==3)
            {
                Test->TestEqual(TEXT("Held Ctrl slow speed"),C->GetCharacterMovement()->MaxWalkSpeed,GetDefault<UHSSettings>()->SlowSpeed);
                Key(PC,EKeys::LeftControl,IE_Released); Next(.1f);
            }
            else if(Stage==4)
            {
                Test->TestFalse(TEXT("Held Ctrl release does not crouch"),C->bIsCrouched);
                Key(PC,EKeys::LeftControl,IE_Pressed); Next(.05f);
            }
            else if(Stage==5) { Key(PC,EKeys::LeftControl,IE_Released); Next(.1f); }
            else if(Stage==6)
            {
                Test->TestTrue(TEXT("Short Ctrl tap crouches"),C->bIsCrouched);
                Screenshot(TEXT("Crouch.png"));
                Key(PC,EKeys::LeftShift,IE_Pressed); Next(.1f);
            }
            else if(Stage==7)
            {
                Test->TestEqual(TEXT("Crouch ignores sprint"),C->GetCharacterMovement()->MaxWalkSpeedCrouched,GetDefault<UHSSettings>()->CrouchSpeed);
                Key(PC,EKeys::LeftShift,IE_Released); C->UnCrouch(); Next(.1f);
            }
            else if(Stage==8)
            {
                // Use an actual map pickup and its stable key, rather than a mocked item.
                AHSPickup* Pickup=nullptr;
                for(TActorIterator<AHSPickup> It(W);It;++It) if(It->PickupId==TEXT("Demo_A_0")) { Pickup=*It; break; }
                if(!Pickup) { Test->AddError(TEXT("Demo_A_0 not found")); return true; }
                CollectedKey=Pickup->GetPersistentKey();
                FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View,Rotation);
                // Position the real map actor under the reticle, in reach, then use
                // the same E input and focus trace as a person playing the game.
                Pickup->SetActorLocation(View+Rotation.Vector()*220.f);
                Stage=80; Until=Now+.15;
            }
            else if(Stage==80)
            {
                Test->TestNotNull(TEXT("Reticle focuses reachable map pickup"),C->FocusedPickup.Get());
                if(C->FocusedPickup)
                {
                    HandDistance=FVector::Distance(C->GetMesh()->GetSocketLocation("hand_r"),C->FocusedPickup->GetActorLocation());
                    UpperArmLength=FVector::Distance(C->GetMesh()->GetSocketLocation("upperarm_r"),C->GetMesh()->GetSocketLocation("lowerarm_r"));
                    ForearmLength=FVector::Distance(C->GetMesh()->GetSocketLocation("lowerarm_r"),C->GetMesh()->GetSocketLocation("hand_r"));
                }
                Key(PC,EKeys::E,IE_Pressed); Stage=81; Until=Now+.1;
            }
            else if(Stage==81)
            {
                Key(PC,EKeys::E,IE_Released);
                Test->TestTrue(TEXT("E pickup stores stable world key"),Session->CollectedPickups.Contains(CollectedKey));
                Test->TestNotNull(TEXT("Item reaches hotbar"),Session->GetSelectedItem());
                Test->TestTrue(TEXT("E pickup triggers visible reach pose"),C->PickupPoseAlpha>0);
                Test->TestTrue(TEXT("IK hand moves toward picked object"),FVector::Distance(C->GetMesh()->GetSocketLocation("hand_r"),C->PickupTargetLocation)<HandDistance-3);
                Test->TestTrue(TEXT("Reach preserves upper arm length"),FMath::Abs(FVector::Distance(C->GetMesh()->GetSocketLocation("upperarm_r"),C->GetMesh()->GetSocketLocation("lowerarm_r"))-UpperArmLength)<.2f);
                Test->TestTrue(TEXT("Reach preserves forearm length"),FMath::Abs(FVector::Distance(C->GetMesh()->GetSocketLocation("lowerarm_r"),C->GetMesh()->GetSocketLocation("hand_r"))-ForearmLength)<.2f);
                PC->ToggleHotbarMouse(); PC->InspectSelected();
                Test->TestTrue(TEXT("Inspect UI active"),PC->IsInspecting());
                Test->TestTrue(TEXT("World paused"),UGameplayStatics::IsGamePaused(W));
                FrozenWorldTime=W->GetTimeSeconds();
                Screenshot(TEXT("Inspection.png"));
                for(TActorIterator<AHSMonster> It(W);It;++It) { Monster=*It; MonsterLocation=It->GetActorLocation(); break; }
                Stage=9; Until=Now+(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))?2.f:.35f);
            }
            else if(Stage==9)
            {
                Test->TestEqual(TEXT("Inspection freezes world time"),W->GetTimeSeconds(),FrozenWorldTime);
                if(Monster.IsValid()) Test->TestTrue(TEXT("Inspection freezes monster"),Monster->GetActorLocation().Equals(MonsterLocation,.01f));
                if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                {
                    Screenshot(TEXT("InspectionReady.png"));
                    Stage=88; Until=Now+.15; return false;
                }
                Stage=89; Until=Now; return false;
            }
            else if(Stage==88)
            {
                Test->TestTrue(TEXT("Inspection widget receives pointer input"),ManipulateInspectionThroughSlate(W));
                Screenshot(TEXT("InspectionRotated.png")); Stage=89; Until=Now+.35;
            }
            else if(Stage==89)
            {
                Test->TestEqual(TEXT("Rotating model keeps game time paused"),W->GetTimeSeconds(),FrozenWorldTime);
                PC->CloseInspection();
                Test->TestFalse(TEXT("Inspection unpauses world"),UGameplayStatics::IsGamePaused(W));
                Test->TestTrue(TEXT("Returns to previous mouse mode"),PC->IsMouseMode());
                // Let Slate rebuild its hit-test grid after the modal disappears.
                Stage=90; Until=Now+.15;
            }
            else if(Stage==90)
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
                {
                    Test->TestTrue(TEXT("Real Slate drag moves item into empty slot"),DragThroughSlate(W));
                    Screenshot(TEXT("Hotbar.png"));
                }
                else Session->SwapSlots(0,5);
                Test->TestEqual(TEXT("UI operation follows selection"),Session->SelectedSlot,5);
                PC->ToggleHotbarMouse();
                if(Monster.IsValid())
                {
                    Monster->SetActorLocation(FVector(1400,1000,110));
                    C->SetActorLocation(FVector(-1700,0,100));
                    OriginalMonsterDistance=FVector::Dist2D(C->GetActorLocation(),Monster->GetActorLocation());
                    ChaseStartedGameTime=W->GetTimeSeconds();
                    FNavLocation MonsterNav,PlayerNav;
                    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
                    const bool MonsterOnNav=Nav && Nav->ProjectPointToNavigation(Monster->GetActorLocation(),MonsterNav);
                    const bool PlayerOnNav=Nav && Nav->ProjectPointToNavigation(C->GetActorLocation(),PlayerNav);
                    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,Monster->GetActorLocation(),C->GetActorLocation(),Monster.Get());
                    Test->TestTrue(TEXT("NavMesh covers far monster and connects to player"),MonsterOnNav && PlayerOnNav && Path && Path->IsValid());
                    Test->AddInfo(FString::Printf(TEXT("Chase start: monster=%s player=%s nav=%d/%d path=%d points=%d"),*Monster->GetActorLocation().ToString(),*C->GetActorLocation().ToString(),MonsterOnNav,PlayerOnNav,Path && Path->IsValid(),Path?Path->PathPoints.Num():0));
                    auto* AI=Cast<AHSPursuitController>(Monster->GetController());
                    Test->TestNotNull(TEXT("Monster AI controller"),AI);
                    Test->TestTrue(TEXT("Monster behavior tree running"),AI && Cast<UBehaviorTreeComponent>(AI->GetBrainComponent()) && AI->GetBrainComponent()->IsRunning());
                }
                Stage=10; Until=Now+3;
            }
            else if(Stage==10)
            {
                if(Monster.IsValid()) Test->AddInfo(FString::Printf(TEXT("Chase result: monster=%s distance=%.2f initial=%.2f state=%d gameTime=%.2f velocity=%s"),*Monster->GetActorLocation().ToString(),FVector::Dist2D(C->GetActorLocation(),Monster->GetActorLocation()),OriginalMonsterDistance,int32(Monster->PursuitState),W->GetTimeSeconds()-ChaseStartedGameTime,*Monster->GetVelocity().ToString()));
                Test->TestTrue(TEXT("Monster follows across navigation"),Monster.IsValid() && FVector::Dist2D(C->GetActorLocation(),Monster->GetActorLocation())<OriginalMonsterDistance-100);
                if(Monster.IsValid()) { Monster->SetActorLocation(C->GetActorLocation()+FVector(450,0,0)); }
                Next(1);
            }
            else if(Stage==11)
            {
                Test->TestTrue(TEXT("Near monster slows"),Monster.IsValid() && Monster->GetCharacterMovement()->MaxWalkSpeed<=GetDefault<UHSSettings>()->NearSpeed+1);
                AHSPortal* Portal=nullptr;
                for(TActorIterator<AHSPortal> It(W);It;++It) if(It->Destination.ToSoftObjectPath().GetLongPackageName().EndsWith(TEXT("Test_B"))) { Portal=*It; break; }
                W->GetGameInstance()->GetSubsystem<UHSProgression>()->CollectClue(TEXT("Key_1"));
                Test->TestTrue(TEXT("Portal accepts travel A to B"),Portal && Portal->Travel(C)); Next(2);
            }
            else if(Stage==12)
            {
                if(UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Test_B")) return false;
                Test->TestTrue(TEXT("Same session across level travel"),PC->GetSession()==Session.Get());
                Test->TestNotNull(TEXT("Hotbar retained in B"),PC->GetSession()->GetSelectedItem());
                Test->TestEqual(TEXT("Selected index retained"),PC->GetSession()->SelectedSlot,5);
                Test->TestTrue(TEXT("Safe arrival spawn tag honored"),FVector::Dist2D(C->GetActorLocation(),FVector(-1800,0,0))<100);
                if(auto* Director=AHSRoomDirector::Find(W)) Director->FinishIntro();
                W->GetGameInstance()->GetSubsystem<UHSProgression>()->CollectClue(TEXT("Key_1"));
                AHSPortal* Portal=nullptr;
                for(TActorIterator<AHSPortal> It(W);It;++It) if(It->Destination.ToSoftObjectPath().GetLongPackageName().EndsWith(TEXT("Test_C"))) { Portal=*It; break; }
                Test->TestTrue(TEXT("Portal accepts travel B to C"),Portal && Portal->Travel(C)); Next(2);
            }
            else if(Stage==13)
            {
                if(UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Test_C")) return false;
                Test->TestNotNull(TEXT("Hotbar retained in C"),PC->GetSession()->GetSelectedItem());
                if(auto* Director=AHSRoomDirector::Find(W)) Director->FinishIntro();
                W->GetGameInstance()->GetSubsystem<UHSProgression>()->CollectClue(TEXT("Key_1"));
                AHSPortal* Portal=nullptr;
                for(TActorIterator<AHSPortal> It(W);It;++It) if(It->Destination.ToSoftObjectPath().GetLongPackageName().EndsWith(TEXT("Test_A"))) { Portal=*It; break; }
                Test->TestTrue(TEXT("Portal accepts travel C to A"),Portal && Portal->Travel(C)); Next(2);
            }
            else if(Stage==14)
            {
                if(UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Test_A")) return false;
                if(auto* Director=AHSRoomDirector::Find(W)) Director->FinishIntro();
                bool Respawned=false;
                for(TActorIterator<AHSPickup> It(W);It;++It) if(It->GetPersistentKey()==CollectedKey) Respawned=true;
                Test->TestFalse(TEXT("Collected pickup does not respawn on return"),Respawned);
                Test->TestNotNull(TEXT("Item still carried on round trip"),PC->GetSession()->GetSelectedItem());
                Test->AddInfo(TEXT("Validated movement, Ctrl modes, mannequin, NavMesh pursuit, depth fade, pickup, paused inspection and A/B/C round-trip persistence."));
                return true;
            }
            return false;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRuntimeTest,"HorrorSystems.Runtime.EndToEnd",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHSRuntimeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FHSRuntimeScenario(this));
    return true;
}
#endif
