#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HSAuthoringLibrary.generated.h"
UCLASS()
class HORRORSYSTEMS_API UHSAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="HorrorSystems|Authoring") static bool CreatePickupAnimationAssets();
};
