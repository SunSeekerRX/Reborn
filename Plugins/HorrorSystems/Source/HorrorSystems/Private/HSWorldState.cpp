#include "HSWorldState.h"
#include "HSPolicy.h"
#include "HSProgression.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "MoviePlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "HSSceneInteractions.h"
#include "HSParticleTitle.h"
#include "HSStoryActors.h"

UHSWorldState::UHSWorldState() { Slots.SetNum(HSPolicy::Capacity); }
void UHSWorldState::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TravelTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UHSWorldState::TickWhiteTravel));
    LoadedMapHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,&UHSWorldState::OnTravelMapLoaded);
}
void UHSWorldState::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TravelTicker);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadedMapHandle);
    if(TravelOverlay.IsValid() && GetGameInstance()->GetGameViewportClient()) GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(TravelOverlay.ToSharedRef());
    TravelOverlay.Reset();Super::Deinitialize();
}
void UHSWorldState::AttachTravelOverlay()
{
    if(TravelOverlay.IsValid() || !GetGameInstance()->GetGameViewportClient()) return;
    TravelOverlay=SNew(SHSTransitionVeil).Alpha_Lambda([this]{return WhiteTravelAlpha;}).Death_Lambda([this]{return bDeathTransition;})
        .Tint_Lambda([this]{return FLinearColor::LerpUsingHSV(FLinearColor::White,FLinearColor::Black,EndingBlackAmount);})
        .Visibility_Lambda([this]{return bWhiteTransition?EVisibility::HitTestInvisible:EVisibility::Collapsed;});
    GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);
}
bool UHSWorldState::BeginWhiteTravel(const FHSRoomRoute& Route,FName SpawnTag,bool bLocal,FName LocalRoom)
{
    auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(bWhiteTransition || bTravelPending || !PC || !Progress->CanExit(Route)) return false;
    PendingRoute=Route;PendingSpawnTag=SpawnTag;PendingLocalRoom=LocalRoom;bPendingLocal=bLocal;bPendingMenu=false;
    bEndingToTitle=false;EndingBlackAmount=0;
    bTravelPending=true;bWhiteTransition=true;WhiteTravelAlpha=0;TravelPhase=1;TravelHold=0;
    PC->CloseInspection();PC->SetGameplayLocked(true);AttachTravelOverlay();return true;
}
bool UHSWorldState::BeginMenuTravel(bool bStartGame)
{
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(!PC || bWhiteTransition || (bStartGame && !bTitleScreen)) return false;
    if(bStartGame) ResetSession();
    bEndingToTitle=!bStartGame;EndingBlackAmount=0;
    PendingRoute=FHSRoomRoute();PendingRoute.Destination=TSoftObjectPtr<UWorld>(FSoftObjectPath(bStartGame?TEXT("/HorrorSystems/Maps/Basic_roomA.Basic_roomA"):TEXT("/HorrorSystems/Maps/RebornTitle.RebornTitle")));
    PendingSpawnTag=bStartGame?FName(TEXT("Safe_A")):NAME_None;bPendingLocal=false;bPendingMenu=true;
    PC->CloseInspection();PC->SetGameplayLocked(true);
    bTravelPending=true;bWhiteTransition=true;WhiteTravelAlpha=0;TravelPhase=1;TravelHold=0;AttachTravelOverlay();return true;
}
bool UHSWorldState::IsInSafety(const AHSCharacter* Player) const
{
    if(!Player) return false;
    if(const auto* Room=AHSRoomDirector::Find(GetWorld());Room && Room->IsSafe(Player)) return true;
    for(TActorIterator<AHSRecoveryCheckpoint> It(GetWorld());It;++It) if(It->SafeArea->Bounds.GetBox().IsInsideOrOn(Player->GetActorLocation())) return true;
    return false;
}
bool UHSWorldState::RecoverAtSafety()
{
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(!PC || !PC->GetPawn() || !bHasRecovery || bWhiteTransition) return false;
    PC->CloseInspection();PC->SetGameplayLocked(true);
    bPendingRecovery=true;bDeathTransition=true;bPendingMenu=false;
    bWhiteTransition=true;bTravelPending=true;WhiteTravelAlpha=0;TravelPhase=1;TravelHold=0;
    AttachTravelOverlay();return true;
}
bool UHSWorldState::ApplySafetyRecovery()
{
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
    auto* Room=AHSRoomDirector::Find(GetWorld());
    if(!Player || !Room || !bHasRecovery) return false;
    Player->CancelAutoInspection();PC->CloseInspection();Lives=3;
    Player->GetCharacterMovement()->StopMovementImmediately();Player->ClearMovementModifiers();Player->UnCrouch();
    Player->LaunchCharacter(FVector::ZeroVector,true,true);
    Player->SetActorTransform(RecoveryTransform,false,nullptr,ETeleportType::TeleportPhysics);
    PC->SetControlRotation(RecoveryTransform.Rotator());Player->HitProtectionRemaining=6.f;
    Room=AHSRoomDirector::Find(GetWorld());
    if(auto* Story=Cast<AHSStoryDirector>(Room);Story && Story->SafeDoor) Story->SafeDoor->Unlock();
    Room->bHasLeftSafeArea=false;Room->bSafeAreaSealed=false;Room->ReturnBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Room->Progress()->bTimerRunning=false;bRecoverySafety=false;
    // A clue already acquired must still authorize the safe return after recovery.
    Room->bHasLeftSafeArea=true;Room->Tick(0);Room->bHasLeftSafeArea=Room->bSafeTravelReady;
    bRecoverySafety=true;return true;
}
void UHSWorldState::OnTravelMapLoaded(UWorld* World)
{
    if(!bWhiteTransition || !World || World->GetGameInstance()!=GetGameInstance()) return;
    WhiteTravelAlpha=1.f;TravelPhase=3;TravelHold=0.f;
    // The session owns this widget, so controller destruction cannot remove the cover.
    if(auto* Viewport=GetGameInstance()->GetGameViewportClient();Viewport && TravelOverlay.IsValid())
    {Viewport->RemoveViewportWidgetContent(TravelOverlay.ToSharedRef());Viewport->AddViewportWidgetContent(TravelOverlay.ToSharedRef(),1000);}
}
bool UHSWorldState::TickWhiteTravel(float Dt)
{
    if(!bWhiteTransition) return true;
    AttachTravelOverlay();
    auto* PC=Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(),0));
    if(TravelPhase==1)
    {
        WhiteTravelAlpha=FMath::Min(1.f,WhiteTravelAlpha+Dt/.18f);
        if(WhiteTravelAlpha>=1.f) {TravelPhase=2;TravelHold=0.f;}
    }
    else if(TravelPhase==2)
    {
        // Keep an opaque frame on screen before triggering a synchronous map load.
        TravelHold+=Dt;if(TravelHold<.12f) return true;
        auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
        if(bPendingRecovery)
        {
            ApplySafetyRecovery();bPendingRecovery=false;TravelPhase=3;TravelHold=0;
        }
        else if(bPendingLocal)
        {
            APlayerStart* Start=nullptr;
            for(TActorIterator<APlayerStart> It(GetWorld());It;++It) if(It->PlayerStartTag==PendingSpawnTag) {Start=*It;break;}
            auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
            if(Start && Player)
            {
                Progress->CommitExit(PendingRoute);Player->ClearMovementModifiers();Player->SelectedPainting.Reset();
                for(TActorIterator<AHSRoomDirector> It(GetWorld());It;++It) if(It->Rules && It->Rules->RoomId==PendingLocalRoom) It->PrepareEntry();
                Player->SetActorLocationAndRotation(Start->GetActorLocation(),Start->GetActorRotation());PC->SetControlRotation(Start->GetActorRotation());
                TravelPhase=3;TravelHold=0;bTravelPending=false;PendingSpawnTag=NAME_None;
            }
            else {TravelPhase=4;bTravelPending=false;PendingSpawnTag=NAME_None;}
        }
        else
        {
            UWorld* Destination=PendingRoute.Destination.LoadSynchronous();
            if(!Destination) {TravelPhase=4;bTravelPending=false;PendingSpawnTag=NAME_None;return true;}
            if(IsMoviePlayerEnabled())
            {
                FLoadingScreenAttributes Screen;Screen.MinimumLoadingScreenDisplayTime=.15f;Screen.bAutoCompleteWhenLoadingCompletes=true;Screen.bMoviesAreSkippable=false;
                Screen.WidgetLoadingScreen=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::White);
                GetMoviePlayer()->SetupLoadingScreen(Screen);
            }
            if(!bPendingMenu) Progress->CommitExit(PendingRoute);TravelPhase=5;
            UGameplayStatics::OpenLevelBySoftObjectPtr(this,PendingRoute.Destination);
        }
    }
    else if(TravelPhase==3)
    {
        auto* Room=AHSRoomDirector::Find(GetWorld());
        if(PC) PC->SetGameplayLocked(true);
        if(PC && (bTitleScreen || (PC->GetPawn() && Room && Room->Rules && Room->Progress()->ActiveRoom==Room->Rules->RoomId)))
        {TravelHold+=Dt;if(bEndingToTitle) EndingBlackAmount=FMath::Clamp(TravelHold/.3f,0.f,1.f);if(TravelHold>(bEndingToTitle?.7f:.35f)) TravelPhase=4;}
    }
    else if(TravelPhase==4)
    {
        WhiteTravelAlpha=FMath::Max(0.f,WhiteTravelAlpha-Dt/.45f);
        if(WhiteTravelAlpha<=0)
        {
            TravelPhase=0;bWhiteTransition=false;bTravelPending=false;PendingSpawnTag=NAME_None;
            bDeathTransition=false;
            if(PC) {const auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();PC->SetGameplayLocked(bTitleScreen || Progress->bCinematic || Progress->bCompleted || IsDefeated());}
        }
    }
    return true;
}
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
    bWhiteTransition=false;bDeathTransition=false;bPendingRecovery=false;WhiteTravelAlpha=0;TravelPhase=0;
    bEndingToTitle=false;EndingBlackAmount=0;
    Lives=3;bHasRecovery=false;bRecoverySafety=false;
    Slots.Empty(); Slots.SetNum(HSPolicy::Capacity); SelectedSlot=0;
    CollectedPickups.Empty(); PendingSpawnTag=NAME_None; bTravelPending=false;
    if(GetGameInstance()) if(auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>()) Progress->ResetProgression();
    OnHotbarChanged.Broadcast();
}
