#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HSGameMode.generated.h"
UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHSGameMode();
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void StartPlay() override;
};
