#include "HSGameMode.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
AHSGameMode::AHSGameMode() { DefaultPawnClass=AHSCharacter::StaticClass(); PlayerControllerClass=AHSPlayerController::StaticClass(); }
AActor* AHSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    auto* S=GetGameInstance()->GetSubsystem<UHSWorldState>();
    if(S && !S->PendingSpawnTag.IsNone())
    {
        for(TActorIterator<APlayerStart> It(GetWorld());It;++It)
            if(It->PlayerStartTag==S->PendingSpawnTag) return *It;
        UE_LOG(LogTemp,Warning,TEXT("HorrorSystems: spawn tag %s absent; falling back to default start"),*S->PendingSpawnTag.ToString());
    }
    for(TActorIterator<APlayerStart> It(GetWorld());It;++It) if(It->PlayerStartTag==TEXT("Default")) return *It;
    for(TActorIterator<APlayerStart> It(GetWorld());It;++It) if(It->PlayerStartTag==TEXT("Safe_A")) return *It;
    return Super::ChoosePlayerStart_Implementation(Player);
}
void AHSGameMode::StartPlay()
{
    Super::StartPlay();
    // Regenerate dynamic navigation across all bounds after loading a demo map.
    // Editor resaves may leave only part of the cached navigation populated.
    if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Nav->Build();
    if(auto* S=GetGameInstance()->GetSubsystem<UHSWorldState>()) { S->bTravelPending=false; S->PendingSpawnTag=NAME_None; }
}
