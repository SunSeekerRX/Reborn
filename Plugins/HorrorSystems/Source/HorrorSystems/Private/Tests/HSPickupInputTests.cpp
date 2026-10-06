#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSRoomActors.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Layout/Children.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UWorld* GameplayWorld() {for(auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) return C.World();return nullptr;}
void Key(AHSPlayerController* PC,FKey K,EInputEvent E) {PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f));}
class FPickupInput : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; int32 Step=0; double Until=FPlatformTime::Seconds()+2;
    TWeakObjectPtr<AHSPickup> Pickup;
    UHSItemData* Item=nullptr;
public:
    explicit FPickupInput(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Until) return false;
        auto* W=GameplayWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* P=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        if(!P) {Test->AddError(TEXT("Gameplay pawn missing"));return true;}
        auto* S=PC->GetSession();
        switch(Step++)
        {
        case 0:
            if(auto* D=AHSRoomDirector::Find(W)) D->FinishIntro();PC->SetGameplayLocked(false);
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCinematicActor=true;It->bCanDamagePlayer=false;It->GetCharacterMovement()->StopMovementImmediately();}
            S->Slots.Init(FHSItemSlot(),10);
            P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(-700,300,194));
            Item=LoadObject<UHSItemData>(nullptr,TEXT("/HorrorSystems/Items/DA_FinalInformation.DA_FinalInformation"));
            Test->TestNotNull(TEXT("Real collectible item data"),Item);if(!Item) return true;
            Pickup=W->SpawnActor<AHSPickup>(FVector(-700,450,245),FRotator::ZeroRotator);
            Pickup->ItemData=Item;Pickup->PickupId=TEXT("InputRegressionFixture");Pickup->bRotateForDemo=false;
            PC->SetControlRotation((Pickup->GetActorLocation()-P->Camera->GetComponentLocation()).Rotation());
            PC->PlayerCameraManager->UpdateCamera(0);P->RefreshInteractionFocus();
            Test->TestEqual(TEXT("Nearby collectible is focused"),P->FocusedPickup.Get(),Pickup.Get());
            Until=FPlatformTime::Seconds()+.2;return false;
        case 1:
            Test->TestTrue(TEXT("Proximity alone does not collect"),Pickup.IsValid() && !S->Slots[0].Item);
            Key(PC,EKeys::E,IE_Pressed);Until=FPlatformTime::Seconds()+.12;return false;
        case 2:
            Key(PC,EKeys::E,IE_Released);
            Test->TestEqual(TEXT("E puts the real item in hotbar"),S->Slots[0].Item.Get(),Item);
            Test->TestFalse(TEXT("Collected world actor disappears"),Pickup.IsValid());
            Until=FPlatformTime::Seconds()+1;return false;
        case 3:
            Test->TestFalse(TEXT("Pickup does not automatically inspect"),PC->IsInspecting());
            Key(PC,EKeys::One,IE_Pressed);Until=FPlatformTime::Seconds()+.08;return false;
        case 4:
            Key(PC,EKeys::One,IE_Released);
            Test->TestEqual(TEXT("Single number selects slot"),S->SelectedSlot,0);
            Test->TestFalse(TEXT("Single number does not inspect"),PC->IsInspecting());
            Key(PC,EKeys::One,IE_Repeat);Until=FPlatformTime::Seconds()+.04;return false;
        case 5:
            Test->TestFalse(TEXT("Holding number is not a double tap"),PC->IsInspecting());
            Key(PC,EKeys::One,IE_Pressed);Until=FPlatformTime::Seconds()+.05;return false;
        case 6:
            Key(PC,EKeys::One,IE_Released);
            Test->TestTrue(TEXT("Double number opens inspection"),PC->IsInspecting());
            Test->TestEqual(TEXT("Correct inventory model inspected"),PC->GetInspectionItem(),Item);
            Test->TestTrue(TEXT("Inspection pauses game"),UGameplayStatics::IsGamePaused(W));
            Until=FPlatformTime::Seconds()+.8;return false;
        case 7:
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/DoubleNumberInspection.png")),true,false);
            Until=FPlatformTime::Seconds()+.3;return false;
        case 8:
            PC->CloseInspection();Test->TestFalse(TEXT("Closing resumes game"),UGameplayStatics::IsGamePaused(W));
            PC->ActivateNumberSlot(9);PC->ActivateNumberSlot(9);Test->TestFalse(TEXT("Empty double tap is harmless"),PC->IsInspecting());
            PC->ActivateNumberSlot(0);PC->ActivateNumberSlot(9);PC->ActivateNumberSlot(0);
            Test->TestFalse(TEXT("Different slots do not count as a double tap"),PC->IsInspecting());
            Until=FPlatformTime::Seconds()+.4;return false;
        case 9:
            PC->ActivateNumberSlot(0);Test->TestFalse(TEXT("Separated taps only select"),PC->IsInspecting());
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))
            {
                TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>,const FString&)> Find;
                Find=[&Find](TSharedRef<SWidget> Widget,const FString& Type)->TSharedPtr<SWidget>
                {
                    if(Widget->GetTypeAsString()==Type) return Widget;
                    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I) if(auto Found=Find(Children->GetChildAt(I),Type)) return Found;
                    return nullptr;
                };
                TSharedPtr<SWidget> Slot,Overlay;
                for(const auto& Window:FSlateApplication::Get().GetTopLevelWindows())
                {if(!Slot) Slot=Find(Window,TEXT("SHSSlot"));if(!Overlay) Overlay=Find(Window,TEXT("SHSOverlay"));}
                Test->TestTrue(TEXT("Actual hotbar Slate widget is present"),Slot.IsValid());
                Test->TestTrue(TEXT("Actual overlay Slate widget is present"),Overlay.IsValid());
                PC->SelectSlot(0);PC->ToggleHotbarMouse();
                if(Overlay)
                {
                    const FKeyEvent Number(EKeys::One,FModifierKeysState(),0,false,0,0);
                    Test->TestTrue(TEXT("Mouse-mode overlay handles a number press"),Overlay->OnKeyDown(Overlay->GetCachedGeometry(),Number).IsEventHandled());
                    Test->TestFalse(TEXT("Overlay single number only selects"),PC->IsInspecting());
                    Overlay->OnKeyDown(Overlay->GetCachedGeometry(),Number);
                    Test->TestTrue(TEXT("Mouse-mode number double tap inspects"),PC->IsInspecting());PC->CloseInspection();
                }
                if(Slot)
                {
                    const FPointerEvent Click(0,FVector2D::ZeroVector,FVector2D::ZeroVector,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
                    Slot->OnMouseButtonDown(Slot->GetCachedGeometry(),Click);
                    Test->TestFalse(TEXT("Single mouse click only selects"),PC->IsInspecting());
                    Test->TestTrue(TEXT("Actual slot handles left double click"),Slot->OnMouseButtonDoubleClick(Slot->GetCachedGeometry(),Click).IsEventHandled());
                    Test->TestTrue(TEXT("Mouse double click opens correct item"),PC->IsInspecting() && PC->GetInspectionItem()==Item);PC->CloseInspection();
                }
                PC->ToggleHotbarMouse();
            }
            return true;
        }
        return false;
    }
};
class FArtCalibration : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;double Until=FPlatformTime::Seconds()+2;
public:
    explicit FArtCalibration(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Until) return false;
        auto* W=GameplayWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        if(!PC) return true;
        if(Step==0) {if(auto* D=AHSRoomDirector::Find(W)) D->FinishIntro();PC->SetGameplayLocked(false);}
        if(Step>=32) {PC->CloseInspection();return true;}
        const bool bNewArt=FParse::Param(FCommandLine::Get(),TEXT("HSNewRoomArt"));
        const TCHAR* Groups[]={bNewArt?TEXT("BookshelfNew"):TEXT("Bookshelf"),bNewArt?TEXT("DiningTable"):TEXT("Drawer"),bNewArt?TEXT("Lantern"):TEXT("Sofa"),bNewArt?TEXT("Bookshelf"):TEXT("Locker")};
        const int32 Index=Step/2,Angle=Index%4*90;const TCHAR* Group=Groups[Index/4];
        if(Step%2==0)
        {
            PC->CloseInspection();auto* Item=NewObject<UHSItemData>(PC);Item->DisplayName=FText::FromString(FString::Printf(TEXT("%s yaw %d"),Group,Angle));
            Item->InspectionMesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/HorrorSystems/Art/%s/SM_%s.SM_%s"),Group,Group,Group));
            Item->InspectionRotation=FRotator(0,Angle,0);Test->TestNotNull(TEXT("Source art"),Item->InspectionMesh.Get());PC->InspectItem(Item);
            Until=FPlatformTime::Seconds()+.5;
        }
        else {FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),FString::Printf(TEXT("Verification/Art_%s_%d.png"),Group,Angle)),true,false);Until=FPlatformTime::Seconds()+.15;}
        ++Step;return false;
    }
};
class FWorldFurniture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;double Until=FPlatformTime::Seconds()+2;
public:
    explicit FWorldFurniture(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Until) return false;
        auto* W=GameplayWorld();auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* P=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;if(!P) {Test->AddError(TEXT("Missing gameplay pawn"));return true;}
        if(Step==0)
        {
            if(auto* D=AHSRoomDirector::Find(W)) D->FinishIntro();PC->SetGameplayLocked(false);
            for(TActorIterator<ASkeletalMeshActor> It(W);It;++It) Test->TestTrue(TEXT("Authored skeletal reference is hidden and nonblocking"),It->IsHidden() && !It->GetActorEnableCollision());
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCinematicActor=true;It->bCanDamagePlayer=false;}
        }
        const bool bNewArt=FParse::Param(FCommandLine::Get(),TEXT("HSNewRoomArt"));
        const TCHAR* Groups[]={bNewArt?TEXT("BookshelfNew"):TEXT("Bookshelf"),bNewArt?TEXT("DiningTable"):TEXT("Drawer"),bNewArt?TEXT("Lantern"):TEXT("Locker")};if(Step>=(bNewArt?10:6)) return true;
        if(Step>=6)
        {
            if(Step%2==0)
            {
                P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(Step==6?FVector(-200,-1000,294):FVector(-700,1100,194));
                PC->SetControlRotation(FRotator(Step==6?0:20,Step==6?0:180,0));PC->PlayerCameraManager->UpdateCamera(0);Until=FPlatformTime::Seconds()+1;
            }
            else {FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),Step==7?TEXT("Verification/World_SafeWall.png"):TEXT("Verification/World_RoomWall.png")),true,false);Until=FPlatformTime::Seconds()+.2;}
            ++Step;return false;
        }
        const TCHAR* Group=Groups[Step/2];
        if(Step%2==0)
        {
            AActor* Furniture=nullptr;
            for(TActorIterator<AActor> It(W);It;++It) if(auto* M=It->FindComponentByClass<UStaticMeshComponent>();M && M->GetStaticMesh() && M->GetStaticMesh()->GetName().StartsWith(FString::Printf(TEXT("SM_%s_%s"),Group,bNewArt?TEXT("NewRoom"):TEXT("FacingV3")))) {Furniture=*It;break;}
            if(!Furniture) {Test->AddError(FString::Printf(TEXT("Missing corrected %s"),Group));return true;}
            FVector C,E;Furniture->GetActorBounds(false,C,E);
            FVector Front=FString(Group)==TEXT("Drawer") && E.X>E.Y?FVector(0,1,0):FVector(C.X < -550?1:-1,0,0);
            P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(C.X+Front.X*250,C.Y+Front.Y*250,194));
            PC->SetControlRotation((C+FVector(0,0,25)-P->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
            Until=FPlatformTime::Seconds()+.6;
        }
        else {FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),FString::Printf(TEXT("Verification/World_%s_Front.png"),Group)),true,false);Until=FPlatformTime::Seconds()+.2;}
        ++Step;return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSPickupInputTest,"HorrorSystems.UI.PickupDoubleNumber",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSPickupInputTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FPickupInput(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSArtCalibrationTest,"HorrorSystems.UI.ArtFrontCalibration",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSArtCalibrationTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FArtCalibration(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSWorldFurnitureTest,"HorrorSystems.Scene.FurnitureFacing",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSWorldFurnitureTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FWorldFurniture(this));return true;}
#endif

