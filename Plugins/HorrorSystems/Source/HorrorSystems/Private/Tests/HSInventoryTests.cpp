#include "Misc/AutomationTest.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "Engine/GameInstance.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSInventoryTest, "HorrorSystems.Inventory.CapacitySwapAndSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHSInventoryTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance=NewObject<UGameInstance>();
    UHSWorldState* Session=NewObject<UHSWorldState>(GameInstance);
    UHSItemData* Data=NewObject<UHSItemData>();
    TestEqual(TEXT("Exactly ten slots"), Session->Slots.Num(), 10);
    TestFalse(TEXT("Null item rejected"), Session->TryAddItem(nullptr,TEXT("null")));
    for(int32 I=0; I<10; ++I) TestTrue(TEXT("Fill slot"),Session->TryAddItem(Data,FString::FromInt(I)));
    TestFalse(TEXT("Eleventh stays in world"),Session->TryAddItem(Data,TEXT("overflow")));
    TestFalse(TEXT("Failed pickup not recorded"),Session->CollectedPickups.Contains(TEXT("overflow")));
    TestFalse(TEXT("Already collected rejected"),Session->TryAddItem(Data,TEXT("0")));
    TestFalse(TEXT("Invalid swap rejected"),Session->SwapSlots(-1,5));
    Session->SelectSlot(0); Session->SwapSlots(0,9);
    TestEqual(TEXT("Selection follows item"),Session->SelectedSlot,9);
    TestEqual(TEXT("Instance follows item"),Session->Slots[9].WorldPickupKey,FString(TEXT("0")));
    TestFalse(TEXT("Invalid selection rejected"),Session->SelectSlot(10));
    TestEqual(TEXT("Selection intact"),Session->SelectedSlot,9);
    Session->ResetSession();
    TestEqual(TEXT("Reset keeps capacity"),Session->Slots.Num(),10);
    TestEqual(TEXT("Reset clears collected items"),Session->CollectedPickups.Num(),0);
    TestNull(TEXT("Reset empty selected slot"),Session->GetSelectedItem());
    TestTrue(TEXT("Pickup possible after reset"),Session->TryAddItem(Data,TEXT("0")));
    Session->SwapSlots(0,5);
    TestNull(TEXT("Swap with empty leaves source empty"),Session->Slots[0].Item.Get());
    TestEqual(TEXT("Selection follows into empty slot"),Session->SelectedSlot,5);
    return true;
}
#endif
