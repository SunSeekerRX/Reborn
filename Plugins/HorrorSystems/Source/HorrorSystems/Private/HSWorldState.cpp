#include "HSWorldState.h"
#include "HSPolicy.h"
#include "HSProgression.h"
#include "Engine/GameInstance.h"

UHSWorldState::UHSWorldState() { Slots.SetNum(HSPolicy::Capacity); }
bool UHSWorldState::LoseLife()
{
    if(Lives<=0) return false;
    Lives=FMath::Clamp(Lives-1,0,3); return true;
}
bool UHSWorldState::TryAddItem(UHSItemData* Item, const FString& WorldKey)
{
    if (!Item || (!WorldKey.IsEmpty() && CollectedPickups.Contains(WorldKey))) return false;
    for (int32 I = 0; I < Slots.Num(); ++I)
    {
        if (!Slots[I].Item)
        {
            Slots[I].Item = Item;
            Slots[I].WorldPickupKey = WorldKey;
            if (!WorldKey.IsEmpty()) CollectedPickups.Add(WorldKey);
            SelectedSlot = I;
            OnHotbarChanged.Broadcast();
            return true;
        }
    }
    return false;
}
bool UHSWorldState::SwapSlots(int32 From, int32 To)
{
    if (!HSPolicy::ValidSlot(From) || !HSPolicy::ValidSlot(To) || Slots.Num()!=HSPolicy::Capacity) return false;
    if (From == To) return true;
    Slots.Swap(From, To);
    if (SelectedSlot == From) SelectedSlot = To;
    else if (SelectedSlot == To) SelectedSlot = From;
    OnHotbarChanged.Broadcast();
    return true;
}
bool UHSWorldState::SelectSlot(int32 Index)
{
    if (!HSPolicy::ValidSlot(Index)) return false;
    SelectedSlot = Index;
    OnHotbarChanged.Broadcast();
    return true;
}
UHSItemData* UHSWorldState::GetSelectedItem() const
{ return Slots.IsValidIndex(SelectedSlot) ? Slots[SelectedSlot].Item.Get() : nullptr; }
void UHSWorldState::ResetSession()
{
    Lives=3;
    Slots.Empty(); Slots.SetNum(HSPolicy::Capacity); SelectedSlot=0;
    CollectedPickups.Empty(); PendingSpawnTag=NAME_None; bTravelPending=false;
    if(GetGameInstance()) if(auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>()) Progress->ResetProgression();
    OnHotbarChanged.Broadcast();
}
