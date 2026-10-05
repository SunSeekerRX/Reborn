#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HSAnimInstance.generated.h"
// Native locomotion blendspace plus component-space leg/arm IK for crouch/reach.
// Replace with your production Animation Blueprint without changing gameplay code.
UCLASS(Blueprintable, Transient)
class HORRORSYSTEMS_API UHSAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<class UBlendSpace> Locomotion;
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
