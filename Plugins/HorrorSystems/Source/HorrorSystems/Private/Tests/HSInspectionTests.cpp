#include "Misc/AutomationTest.h"
#include "HSInspectionView.h"
#include "HSItemData.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSInspection3DTest,"HorrorSystems.UI.Inspection3D",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSInspection3DTest::RunTest(const FString&)
{
    UWorld* W=nullptr;
    for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
    auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
    if(!PC) {AddError(TEXT("No gameplay player controller"));return false;}
    if(auto* Room=AHSRoomDirector::Find(W)) Room->FinishIntro();
    PC->SetGameplayLocked(false);
    const TCHAR* Names[]={TEXT("Note"),TEXT("Key"),TEXT("Battery"),TEXT("Token")};
    TSharedRef<SHSInspectionView> View=SNew(SHSInspectionView);
    for(const TCHAR* Name:Names)
    {
        auto* Item=LoadObject<UHSItemData>(nullptr,*FString::Printf(TEXT("/HorrorSystems/Items/DA_%s.DA_%s"),Name,Name));
        if(!Item) {AddError(TEXT("Missing item data"));continue;}
        TestNotNull(TEXT("Existing item has a real inspection model"),Item->InspectionMesh.Get());
        PC->InspectItem(Item); View->SetItem(Item);
        auto* Player=Cast<AHSCharacter>(PC->GetPawn());
        TestTrue(TEXT("Player body cannot obstruct inspection"),Player && Player->GetMesh()->bOwnerNoSee);
        TestTrue(TEXT("Inspection pauses gameplay"),UGameplayStatics::IsGamePaused(W));
        TestTrue(TEXT("Isolated preview created"),View->HasModel());
        TestTrue(TEXT("Transparent UI material available"),View->HasDisplayMaterial());
        TestNotNull(TEXT("3D preview has render target"),View->GetRenderTarget());
        const FQuat Initial=View->GetModelRotation();
        View->RotateModel(FVector2D(90,45));
        TestFalse(TEXT("Both rotation axes change model while paused"),View->GetModelRotation().Equals(Initial));
        TestTrue(TEXT("Quaternion stays normalized"),View->GetModelRotation().IsNormalized());
        View->ZoomModel(100); TestEqual(TEXT("Zoom in is bounded"),View->GetZoom(),2.5f);
        View->ZoomModel(-100); TestEqual(TEXT("Zoom out is bounded"),View->GetZoom(),.6f);
        View->ResetView(); TestTrue(TEXT("Reset restores original orientation"),View->GetModelRotation().Equals(Initial));
        TestEqual(TEXT("Reset restores zoom"),View->GetZoom(),1.f);
        CollectGarbage(RF_NoFlags);
        TestNotNull(TEXT("Render target survives garbage collection"),View->GetRenderTarget());
        PC->CloseInspection(); View->SetItem(nullptr);
        TestFalse(TEXT("First-person body is restored after inspection"),Player && Player->GetMesh()->bOwnerNoSee);
        TestFalse(TEXT("Closing unpauses gameplay"),UGameplayStatics::IsGamePaused(W));
        TestFalse(TEXT("Closing releases preview scene"),View->HasModel());
        TestNull(TEXT("Closing releases render target"),View->GetRenderTarget());
    }
    return true;
}
#endif
