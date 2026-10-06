#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
class FPreviewScene;
class UHSItemData;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

// An isolated runtime scene; rendering and input continue while gameplay is paused.
class SHSInspectionView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHSInspectionView) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SHSInspectionView() override;
    void SetItem(UHSItemData* Item);
    void RotateModel(FVector2D Delta);
    void ZoomModel(float WheelDelta);
    void ResetView();
    FQuat GetModelRotation() const { return Rotation; }
    float GetZoom() const { return Zoom; }
    bool HasModel() const { return Scene.IsValid(); }
    bool HasDisplayMaterial() const { return DisplayMaterial.IsValid(); }
    UTextureRenderTarget2D* GetRenderTarget() const { return Target.Get(); }
    virtual void Tick(const FGeometry& G,double Time,float Dt) override;
    virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override;
    virtual FReply OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) override;
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override;
    virtual FReply OnMouseWheel(const FGeometry& G,const FPointerEvent& E) override;
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E) override;
private:
    void UpdateModel();
    TUniquePtr<FPreviewScene> Scene;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    TStrongObjectPtr<UMaterialInstanceDynamic> DisplayMaterial;
    UStaticMeshComponent* Mesh=nullptr;
    USceneCaptureComponent2D* Capture=nullptr;
    TWeakObjectPtr<UHSItemData> CurrentItem;
    FSlateBrush Brush;
    FQuat Rotation=FQuat::Identity;
    FVector Scale=FVector::OneVector;
    FVector Center=FVector::ZeroVector;
    float Distance=200.f;
    float Radius=30.f;
    float Zoom=1.f;
    bool bDragging=false;
    bool bDirty=false;
    int32 WarmupFrames=0;
};
