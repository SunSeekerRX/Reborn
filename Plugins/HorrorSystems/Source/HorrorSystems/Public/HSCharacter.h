#pragma once
#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Character.h"
#include "HSCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UPointLightComponent;
class USoundBase;
class UAnimMontage;
class AHSPickup;
class AHSMonster;

UCLASS(Blueprintable)
class HORRORSYSTEMS_API AHSCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AHSCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void OnStartCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> Camera;
    virtual void Landed(const FHitResult& Hit) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision") TObjectPtr<USpotLightComponent> Flashlight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vision") TObjectPtr<UPointLightComponent> PlayerAuraLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> FootstepSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimMontage> PickupMontage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction") TObjectPtr<AHSPickup> FocusedPickup;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Interaction") TObjectPtr<AActor> FocusedInteraction;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Interaction") TWeakObjectPtr<class AHSSwapPainting> SelectedPainting;
    bool GetInteractionPromptLocation(FVector& Position) const;
    void RefreshInteractionFocus() { RefreshFocus(); }
    void SelectPainting(class AHSSwapPainting* Painting);
    class UMeshComponent* GetHighlightedMesh() const { return HighlightedMesh.Get(); }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation") float PickupPoseAlpha=0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation") FVector PickupTargetLocation=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") float CameraFOVTarget=85.f;
    UFUNCTION(BlueprintCallable, Category="Interaction") void Interact();
    UFUNCTION(BlueprintImplementableEvent, Category="Interaction") void OnItemPickedUp(class UHSItemData* Item);
    void ClearMovementModifiers();
    UFUNCTION(BlueprintCallable, Category="Health") bool ReceiveMonsterContact(AHSMonster* Monster);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health") float HitProtectionRemaining=0.f;
    bool IsSlowWalking() const;
    void StartPickupAnimation(class UHSItemData* Item,const FVector& Target);
    void CancelAutoInspection() { PendingInspection=nullptr; }
protected:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void ZoomIn();
    void ZoomOut();
    void ChangeZoom(float Direction);
    void SprintPressed();
    void SprintReleased();
    void CtrlPressed();
    void CtrlReleased();
    void RefreshFocus();
    void RefreshSpeed();
    bool bSprintHeld=false;
    bool bCtrlHeld=false;
    double CtrlPressedAt=0;
    float PickupTimeLeft=0.f;
    float PickupAnimationDuration=.7f;
    float FootstepDistance=0.f;
    UPROPERTY() TObjectPtr<class UHSItemData> PendingInspection;
    float SmoothedEyeOffset=68.f;
    TWeakObjectPtr<class UMeshComponent> HighlightedMesh;
    UPROPERTY() TObjectPtr<class UMaterialInterface> PickupHighlight;
    UPROPERTY() TObjectPtr<class UMaterialInterface> PaintingHighlight;
    void ApplyInteractionHighlight(AActor* Target);
};
