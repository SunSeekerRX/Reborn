#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSItemData.generated.h"

class UTexture2D;
class USoundBase;

UCLASS(BlueprintType)
class HORRORSYSTEMS_API UHSItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true)) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TObjectPtr<UTexture2D> InspectionImage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inspection 3D") TObjectPtr<class UStaticMesh> InspectionMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inspection 3D") TArray<TObjectPtr<class UMaterialInterface>> InspectionMaterials;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inspection 3D") FVector InspectionScale=FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inspection 3D") FRotator InspectionRotation=FRotator(0,0,0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") bool bInspectOnPickup = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> PickupSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> InspectSound;
};

USTRUCT(BlueprintType)
struct HORRORSYSTEMS_API FHSItemSlot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHSItemData> Item;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString WorldPickupKey;
};
