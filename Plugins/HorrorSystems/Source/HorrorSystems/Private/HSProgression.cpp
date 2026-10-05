#include "HSProgression.h"
#include "HSWorldState.h"
#include "Engine/GameInstance.h"
const FHSRoomRoute* UHSRoomRules::RouteFor(int32 S) const
{ for(const auto& R:Routes) if(R.Stage==S) return &R; return nullptr; }
void UHSProgression::EnterRoom(FName Room,float Duration)
{
    ActiveRoom=Room; Budgets.FindOrAdd(Room)=FMath::Max(1.f,Duration);
    Remaining.FindOrAdd(Room)=Budgets[Room]; bTimerRunning=false; bCinematic=false;
    if(auto* Inventory=GetGameInstance()->GetSubsystem<UHSWorldState>())
    { EntrySlots=Inventory->Slots; EntryPickups=Inventory->CollectedPickups; EntrySelection=Inventory->SelectedSlot; }
    EntryClues=Clues;
}
void UHSProgression::BeginCountdown() { if(!bCompleted && !bCinematic && !ActiveRoom.IsNone()) bTimerRunning=true; }
bool UHSProgression::AdvanceClock(float Dt)
{
    if(!bTimerRunning || bCinematic || bCompleted || Dt<=0) return false;
    float& Time=Remaining.FindOrAdd(ActiveRoom);
    Time=FMath::Max(0.f,Time-Dt);
    if(Time>0) return false;
    bTimerRunning=false; return true;
}
float UHSProgression::GetRemaining() const { const auto* T=Remaining.Find(ActiveRoom); return T?*T:60.f; }
FString UHSProgression::ClueKey(FName C) const { return FString::Printf(TEXT("%d:%s:%s"),Stage,*ActiveRoom.ToString(),*C.ToString()); }
void UHSProgression::CollectClue(FName C) { if(!C.IsNone() && !bCompleted) Clues.Add(ClueKey(C)); }
bool UHSProgression::HasClue(FName C) const { return !C.IsNone() && Clues.Contains(ClueKey(C)); }
bool UHSProgression::CanExit(const FHSRoomRoute& R) const
{
    if(bCompleted || bCinematic || R.Stage!=Stage) return false;
    for(FName C:R.RequiredClues) if(!HasClue(C)) return false;
    return true;
}
void UHSProgression::CommitExit(const FHSRoomRoute& R)
{
    if(!CanExit(R)) return;
    bTimerRunning=false;
    if(R.bFinishAtFinalStage && Stage==3) bCompleted=true;
    else if(R.bAdvanceStage) { Stage=FMath::Min(3,Stage+1); Remaining=Budgets; }
}
void UHSProgression::RollbackRoom()
{
    if(auto* Inventory=GetGameInstance()->GetSubsystem<UHSWorldState>())
    { Inventory->Slots=EntrySlots; Inventory->CollectedPickups=EntryPickups; Inventory->SelectedSlot=EntrySelection; Inventory->OnHotbarChanged.Broadcast(); }
    Clues=EntryClues; Remaining=Budgets; bTimerRunning=false; bCinematic=false;
}
bool UHSProgression::SetStage(int32 S)
{ if(S<1 || S>3 || bCompleted) return false; Stage=S; Remaining=Budgets; return true; }
void UHSProgression::ResetProgression()
{ Stage=1; ActiveRoom=NAME_None; bTimerRunning=bCinematic=bCompleted=false; Remaining.Empty(); Budgets.Empty(); Clues.Empty(); SeenWindows.Empty(); EntrySlots.Empty(); EntryPickups.Empty(); EntryClues.Empty(); EntrySelection=0; }
