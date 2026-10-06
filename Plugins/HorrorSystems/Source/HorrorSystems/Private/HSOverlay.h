#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
class AHSPlayerController;
class UHSItemData;
class SHSInspectionView;
class SHSOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHSOverlay) {} SLATE_ARGUMENT(TWeakObjectPtr<AHSPlayerController>, Controller) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual void Tick(const FGeometry& Geometry,double Time,float Dt) override;
    void SetHovered(int32 Slot) { HoveredSlot=Slot; }
private:
    TWeakObjectPtr<AHSPlayerController> Controller;
    int32 HoveredSlot=INDEX_NONE;
    FVector2D TooltipPosition;
    FVector2D InteractionPromptPosition;
    bool bInteractionPromptVisible=false;
    TSharedPtr<SHSInspectionView> InspectionView;
    FSlateBrush MobiusBrush;
    UHSItemData* HoverItem() const;
    FText CompassText() const;
    FText CountdownText() const;
    FText ClueText() const;
    EVisibility GameplayVisibility() const;
    bool CinematicPlaying() const;
    float EndTime() const;
};
