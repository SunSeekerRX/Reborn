#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSSceneInteractions.h"
#include "HSWorldActors.h"
#include "HSAI.h"
#include "HSSettings.h"
#include "HSProgression.h"
#include "HSWorldState.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
class FTableLightingCheck : public IAutomationLatentCommand {
 FAutomationTestBase* Test;int Step=0;double Until=FPlatformTime::Seconds()+3;
 TWeakObjectPtr<AHSPickup> Item;
public:
 FTableLightingCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override {
  if(FPlatformTime::Seconds()<Until) return false;
  UWorld* W=nullptr;for(auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game) {W=C.World();break;}
  auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
  auto* P=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;if(!P){Test->AddError(TEXT("Missing player"));return true;}
  if(Step==0){
   auto* D=AHSRoomDirector::Find(W);D->FinishIntro();D->Progress()->SetStage(3);PC->SetGameplayLocked(false);
   Test->TestTrue(TEXT("Player point light is dimmer than old 900 lm"),P->PlayerAuraLight->Intensity<=180.f);
   Test->TestTrue(TEXT("Player spotlight is dimmer than old 1200"),P->Flashlight->Intensity<=180.f);
   for(TActorIterator<AHSMonster> It(W);It;++It){
    It->bCanDamagePlayer=false;It->bCinematicActor=true;
    Test->TestNotNull(TEXT("Monster has attached red light"),It->RedAuraLight.Get());
    Test->TestTrue(TEXT("Monster red light follows monster root"),It->RedAuraLight->GetAttachParent()==It->GetRootComponent());
    auto C=It->RedAuraLight->GetLightColor();Test->TestTrue(TEXT("Monster light is red and dim"),C.R>C.G*10 && C.R>C.B*10 && It->RedAuraLight->Intensity<=65);
   }
   for(TActorIterator<AHSPickup> It(W);It;++It) It->Tick(0);
   for(TActorIterator<AHSInspectTrigger> It(W);It;++It) It->Tick(0);
   int Items=0;
   for(TActorIterator<AActor> It(W);It;++It){
    if(!It->IsA<AHSPickup>() && !It->IsA<AHSInspectTrigger>())continue;
    auto* M=It->FindComponentByClass<UStaticMeshComponent>();if(!M)continue;
    auto B=M->Bounds;FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(TableSurface),true,*It);
    Test->TestTrue(TEXT("Collectible has a surface directly beneath it"),W->LineTraceSingleByChannel(Hit,B.Origin+FVector(0,0,-B.BoxExtent.Z+.1),B.Origin-FVector(0,0,B.BoxExtent.Z+15),ECC_Visibility,Params));
    Test->TestTrue(TEXT("Collectible rests on an existing dining table"),Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("Reborn_NewDiningTable")));++Items;
    if(auto* Pickup=Cast<AHSPickup>(*It);Pickup && Pickup->MinimumStage==3)Item=Pickup;
   }
   Test->TestTrue(TEXT("Table surface check covers collectible actors"),Items>0);Test->TestTrue(TEXT("Third stage pickup exists"),Item.IsValid());
   if(!Item.IsValid())return true;
   Item->Tick(0);auto C=Item->Mesh->Bounds.Origin;
   P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(C.X+100,C.Y,194));P->SetActorRotation(FRotator(0,180,0));
   PC->SetControlRotation((C-P->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);P->RefreshInteractionFocus();
   Test->AddInfo(FString::Printf(TEXT("Tabletop expected=%s focused=%s"),*GetNameSafe(Item.Get()),*GetNameSafe(P->FocusedInteraction.Get())));
   Test->TestEqual(TEXT("Tabletop key can be focused"),P->FocusedPickup.Get(),Item.Get());
   FVector Anchor;Test->TestTrue(TEXT("Focused tabletop key exposes E prompt"),P->GetInteractionPromptLocation(Anchor));
   Test->TestTrue(TEXT("E prompt is above mesh, not hidden text label"),Anchor.Z>Item->Mesh->Bounds.Origin.Z && Anchor.Z<Item->Mesh->Bounds.Origin.Z+Item->Mesh->Bounds.BoxExtent.Z+20);
   Until=FPlatformTime::Seconds()+1;++Step;return false;
  }
  if(Step==1){FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/TabletopE_Prompt.png")),true,false);Until=FPlatformTime::Seconds()+.2;++Step;return false;}
  if(Step==2){
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));Until=FPlatformTime::Seconds()+.15;++Step;return false;
  }
  if(Step==3){
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
   Test->TestTrue(TEXT("E collects the tabletop key into inventory"),PC->GetSession()->Slots.ContainsByPredicate([](const FHSItemSlot& S){return S.Item!=nullptr;}));
   P->RefreshInteractionFocus();FVector Anchor;Test->TestFalse(TEXT("Collected item no longer has E prompt"),P->GetInteractionPromptLocation(Anchor));
   for(TActorIterator<AHSMonster> It(W);It;++It) if(!It->IsHidden()){
    auto C=It->GetActorLocation();P->SetActorLocation(C+FVector(230,0,0));PC->SetControlRotation((C+FVector(0,0,30)-P->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);break;
   }
   Until=FPlatformTime::Seconds()+1;++Step;return false;
  }
  FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/MonsterDimRed.png")),true,false);return true;
 }
}; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSLightingTableTest,"HorrorSystems.Scene.LightingTableInteraction",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSLightingTableTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTableLightingCheck(this));return true;}
#endif

