#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "HSProgression.h"
#include "HSWorldState.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSAI.h"
#include "HSSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraComponent.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSProgressionPresence,"HorrorSystems.Progression.IntegrationAvailable",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ProductFilter)
bool FHSProgressionPresence::RunTest(const FString&)
{
    TestNotNull(TEXT("Cross-level stage manager exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSProgression")));
    TestNotNull(TEXT("Room safety and countdown director exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSRoomDirector")));
    TestNotNull(TEXT("Window cinematic exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSWindowSequence")));
    TestNotNull(TEXT("Movable interactive prop exists"),FindObject<UClass>(nullptr,TEXT("/Script/HorrorSystems.HSMovableProp")));
    return true;
}
namespace
{
UWorld* ProgressionWorld()
{ for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) return C.World(); return nullptr; }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSProgressionRulesTest,"HorrorSystems.Progression.TimerRollbackAndStages",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSProgressionRulesTest::RunTest(const FString&)
{
    auto* W=ProgressionWorld(); if(!W) { AddError(TEXT("No game world")); return false; }
    auto* Inventory=W->GetGameInstance()->GetSubsystem<UHSWorldState>();
    auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
    Inventory->ResetSession();
    auto* Item=LoadObject<UHSItemData>(nullptr,TEXT("/HorrorSystems/Items/DA_Key.DA_Key"));
    Inventory->TryAddItem(Item,TEXT("PriorRoom:Keep"));
    P->EnterRoom(TEXT("RoomA"),60);
    TestFalse(TEXT("Safe area does not consume time"),P->AdvanceClock(20)); TestEqual(TEXT("Safe 60 seconds"),P->GetRemaining(),60.f);
    P->BeginCountdown(); P->AdvanceClock(5); TestEqual(TEXT("Room A runs"),P->GetRemaining(),55.f);
    P->bCinematic=true; P->AdvanceClock(10); TestEqual(TEXT("Cinematic freezes clock"),P->GetRemaining(),55.f); P->bCinematic=false;
    Inventory->TryAddItem(Item,TEXT("RoomA:New")); P->CollectClue(TEXT("Key_1"));
    FHSRoomRoute R; R.RequiredClues.Add(TEXT("Key_1"));
    TestTrue(TEXT("Key unlocks correct stage exit"),P->CanExit(R)); R.Stage=2; TestFalse(TEXT("Wrong stage route rejected"),P->CanExit(R)); R.Stage=1;
    P->RollbackRoom();
    TestTrue(TEXT("Previous-room inventory retained"),Inventory->CollectedPickups.Contains(TEXT("PriorRoom:Keep")));
    TestFalse(TEXT("Current-room pickup rolled back"),Inventory->CollectedPickups.Contains(TEXT("RoomA:New")));
    TestFalse(TEXT("Current-room clue rolled back"),P->HasClue(TEXT("Key_1")));
    TestEqual(TEXT("Time reset"),P->GetRemaining(),60.f);
    P->BeginCountdown(); P->AdvanceClock(3); P->EnterRoom(TEXT("RoomB"),60); P->BeginCountdown(); P->AdvanceClock(7);
    TestEqual(TEXT("Room A clock independent"),P->Remaining[TEXT("RoomA")],57.f); TestEqual(TEXT("Room B clock independent"),P->GetRemaining(),53.f);
    TestTrue(TEXT("Expiry triggers"),P->AdvanceClock(60)); TestFalse(TEXT("Expiry triggers only once"),P->AdvanceClock(1));
    P->RollbackRoom(); TestEqual(TEXT("All clocks reset A"),P->Remaining[TEXT("RoomA")],60.f); TestEqual(TEXT("All clocks reset B"),P->Remaining[TEXT("RoomB")],60.f);
    P->CollectClue(TEXT("Key_1")); R.bAdvanceStage=true; P->CommitExit(R); TestEqual(TEXT("C exit advances stage"),P->Stage,2);
    TestFalse(TEXT("Stage-specific clue does not leak"),P->HasClue(TEXT("Key_1")));
    TestFalse(TEXT("Invalid stage rejected"),P->SetStage(4)); P->SetStage(3); R.Stage=3; R.bFinishAtFinalStage=true; P->CollectClue(TEXT("Key_1")); P->CommitExit(R);
    TestTrue(TEXT("Final stage ends game"),P->bCompleted); TestFalse(TEXT("Completed game cannot travel"),P->CanExit(R));
    Inventory->ResetSession(); return true;
}


#endif
