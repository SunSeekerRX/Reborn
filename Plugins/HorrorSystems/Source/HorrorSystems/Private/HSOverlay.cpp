#include "HSOverlay.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSProgression.h"
#include "HSRoomActors.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Input/DragAndDrop.h"
#include "Engine/Texture2D.h"

namespace
{
    const FLinearColor Accent(.68f,.82f,.76f,1.f);
    const FLinearColor Ink(.018f,.025f,.031f,.94f);
    FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular",Size); }
    class FHSSlotDrag : public FDragDropOperation
    {
    public:
        DRAG_DROP_OPERATOR_TYPE(FHSSlotDrag,FDragDropOperation)
        int32 From=0;
        TWeakObjectPtr<UHSWorldState> Session;
        TSharedPtr<SWidget> Decorator;
        virtual TSharedPtr<SWidget> GetDefaultDecorator() const override { return Decorator; }
        virtual FVector2D GetDecoratorPosition() const override { return FSlateApplication::Get().GetCursorPos()+FVector2D(14,14); }
        static TSharedRef<FHSSlotDrag> New(int32 Index,UHSWorldState* S)
        {
            TSharedRef<FHSSlotDrag> Op=MakeShared<FHSSlotDrag>();
            Op->From=Index; Op->Session=S;
            Op->bCreateNewWindow=false;
            Op->Decorator=SNew(SBorder).Padding(8).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
                [SNew(STextBlock).Text(S->Slots[Index].Item->DisplayName).Font(Font(12)).ColorAndOpacity(Accent)];
            Op->Construct(); return Op;
        }
    };
    class SHSSlot : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SHSSlot) {} SLATE_ARGUMENT(int32,Index) SLATE_ARGUMENT(TWeakObjectPtr<AHSPlayerController>,Controller) SLATE_ARGUMENT(TWeakPtr<SHSOverlay>,Overlay) SLATE_END_ARGS()
        int32 Index=0;
        TWeakObjectPtr<AHSPlayerController> PC;
        TWeakPtr<SHSOverlay> Overlay;
        FSlateBrush Brush;
        UHSItemData* LastItem=nullptr;
        bool bHovered=false;
        UHSItemData* Item() const { auto* S=PC.IsValid()?PC->GetSession():nullptr; return S&&S->Slots.IsValidIndex(Index)?S->Slots[Index].Item.Get():nullptr; }
        void Construct(const FArguments& Args)
        {
            Index=Args._Index; PC=Args._Controller; Overlay=Args._Overlay;
            Brush.DrawAs=ESlateBrushDrawType::Image; Brush.ImageSize=FVector2D(36,36);
            ChildSlot
            [SNew(SBox).WidthOverride(64).HeightOverride(72)
                [SNew(SBorder).Padding(2).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor_Lambda([this] {
                        auto* S=PC.IsValid()?PC->GetSession():nullptr;
                        return S && S->SelectedSlot==Index ? Accent : bHovered ? FLinearColor(.38f,.47f,.45f,1) : FLinearColor(.13f,.18f,.2f,1);
                    })
                    [SNew(SBorder).Padding(4).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
                        [SNew(SVerticalBox)
                            +SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [SNew(SBox).WidthOverride(32).HeightOverride(32)[SNew(SImage).Image(&Brush).Visibility_Lambda([this]{return Item()&&Item()->Icon?EVisibility::HitTestInvisible:EVisibility::Hidden;})]]
                        ]
                    ]
                ]
            ];
        }
        virtual void Tick(const FGeometry& G,double T,float Dt) override
        {
            SCompoundWidget::Tick(G,T,Dt);
            if(Item()!=LastItem) { LastItem=Item(); Brush.SetResourceObject(LastItem?LastItem->Icon.Get():nullptr); }
        }
        virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
        {
            if(E.GetEffectingButton()==EKeys::LeftMouseButton && PC.IsValid() && !PC->IsInspecting())
            {
                PC->SelectSlot(Index);
                return FReply::Handled().DetectDrag(SharedThis(this),EKeys::LeftMouseButton);
            }
            if(E.GetEffectingButton()==EKeys::RightMouseButton && PC.IsValid()) { PC->SelectSlot(Index); PC->InspectSelected(); return FReply::Handled(); }
            return FReply::Unhandled();
        }
        virtual FReply OnDragDetected(const FGeometry&,const FPointerEvent&) override
        {
            if(PC.IsValid() && Item() && !PC->IsInspecting()) return FReply::Handled().BeginDragDrop(FHSSlotDrag::New(Index,PC->GetSession()));
            return FReply::Unhandled();
        }
        virtual FReply OnDrop(const FGeometry&,const FDragDropEvent& E) override
        {
            const auto Op=E.GetOperationAs<FHSSlotDrag>();
            if(Op.IsValid() && PC.IsValid() && !PC->IsInspecting() && Op->Session==PC->GetSession())
            { PC->GetSession()->SwapSlots(Op->From,Index); return FReply::Handled(); }
            return FReply::Unhandled();
        }
        virtual void OnMouseEnter(const FGeometry& G,const FPointerEvent& E) override
        { SCompoundWidget::OnMouseEnter(G,E); bHovered=true; if(auto O=Overlay.Pin()) O->SetHovered(Index); }
        virtual void OnMouseLeave(const FPointerEvent& E) override
        { SCompoundWidget::OnMouseLeave(E); bHovered=false; if(auto O=Overlay.Pin()) O->SetHovered(INDEX_NONE); }
    };
}

void SHSOverlay::Construct(const FArguments& Args)
{
    Controller=Args._Controller;
    InspectionBrush.DrawAs=ESlateBrushDrawType::Image;
    InspectionBrush.ImageSize=FVector2D(560,340);
    TSharedRef<SHorizontalBox> Bar=SNew(SHorizontalBox);
    for(int32 I=0;I<10;++I)
        Bar->AddSlot().AutoWidth().Padding(3,0)[SNew(SHSSlot).Index(I).Controller(Controller).Overlay(SharedThis(this))];
    ChildSlot[SNew(SOverlay)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,24)
        [SNew(SVerticalBox).Visibility(this,&SHSOverlay::GameplayVisibility)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SBorder).Padding(FMargin(18,10)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
                [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(this,&SHSOverlay::CountdownText).Font(Font(20)).ColorAndOpacity(Accent)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(SBox).WidthOverride(320).HeightOverride(6)
                        [SNew(SProgressBar).Percent_Lambda([this]() -> TOptional<float> { auto* D=Controller.IsValid()?AHSRoomDirector::Find(Controller->GetWorld()):nullptr; return D && D->Rules?FMath::Clamp(D->Progress()->GetRemaining()/D->Rules->Duration,0.f,1.f):1.f; }).FillColorAndOpacity(Accent)]]
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().Padding(0,8).HAlign(HAlign_Center)[SNew(STextBlock).Text(this,&SHSOverlay::CompassText).Font(Font(12)).ColorAndOpacity(Accent)]
        ]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24,120,0,0)
        [SNew(SBorder).Visibility(this,&SHSOverlay::GameplayVisibility).Padding(18).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.02f,.03f,.04f,.72f))
            [SNew(SBox).WidthOverride(230)[SNew(STextBlock).Text(this,&SHSOverlay::ClueText).Font(Font(14)).WrapTextAt(230).ColorAndOpacity(Accent)]]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(STextBlock).Text(FText::FromString(TEXT("·"))).Font(Font(25)).ColorAndOpacity(Accent).Visibility_Lambda([this]{return GameplayVisibility()!=EVisibility::SelfHitTestInvisible || (Controller.IsValid()&&Controller->IsMouseMode())?EVisibility::Hidden:EVisibility::HitTestInvisible;})]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,28)
        [SNew(SBox).Visibility(this,&SHSOverlay::GameplayVisibility)[Bar]]
        +SOverlay::Slot()[SNew(SCanvas).Visibility(EVisibility::SelfHitTestInvisible)
            +SCanvas::Slot().Position_Lambda([this]{return TooltipPosition;}).Size(FVector2D(300,145))
            [SNew(SBorder).Padding(14).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.035f,.04f,.88f))
                .Visibility_Lambda([this]{return HoverItem() && Controller.IsValid() && !Controller->IsInspecting() ? EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return HoverItem()?HoverItem()->DisplayName:FText::GetEmpty();}).Font(Font(16)).ColorAndOpacity(Accent)]
                    +SVerticalBox::Slot().FillHeight(1).Padding(0,8)[SNew(STextBlock).Text_Lambda([this]{return HoverItem()?HoverItem()->Description:FText::GetEmpty();}).Font(Font(11)).AutoWrapText(true)]
                ]
            ]
        ]
        +SOverlay::Slot()
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.75f)).Padding(28)
            .Visibility_Lambda([this]{return Controller.IsValid()&&Controller->IsInspecting()?EVisibility::Visible:EVisibility::Collapsed;})
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(640)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(24)
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [SNew(SBox).WidthOverride(550).HeightOverride(280)
                            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&InspectionBrush)]]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,8)[SNew(STextBlock).Text_Lambda([this]{auto* I=Controller.IsValid()?Controller->GetInspectionItem():nullptr;return I?I->DisplayName:FText::GetEmpty();}).Font(Font(22)).ColorAndOpacity(Accent)]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(120)
                            [SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).Text_Lambda([this]{auto* I=Controller.IsValid()?Controller->GetInspectionItem():nullptr;return I?I->Description:FText::GetEmpty();}).Font(Font(14)).WrapTextAt(570)]]]
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,18,0,0)
                        [SNew(SButton).OnClicked_Lambda([this]{if(Controller.IsValid()) Controller->CloseInspection(); return FReply::Handled();})
                            [SNew(STextBlock).Text(FText::FromString(TEXT("关闭"))).Font(Font(12))]]
                    ]
                ]
            ]
        ]
        +SOverlay::Slot().VAlign(VAlign_Top)
        [SNew(SBox).HeightOverride_Lambda([this]{return GetCachedGeometry().GetLocalSize().Y*.12f;}).Visibility_Lambda([this]{return CinematicPlaying()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)]]
        +SOverlay::Slot().VAlign(VAlign_Bottom)
        [SNew(SBox).HeightOverride_Lambda([this]{return GetCachedGeometry().GetLocalSize().Y*.12f;}).Visibility_Lambda([this]{return CinematicPlaying()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)]]
        +SOverlay::Slot()
        [SNew(SBorder).Visibility_Lambda([this]{return EndTime()>0?EVisibility::Visible:EVisibility::Collapsed;})
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.02f,.025f,1))
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SVerticalBox).Visibility_Lambda([this]{return EndTime()>1.5f?EVisibility::Visible:EVisibility::Hidden;})
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("黎明已至"))).Font(Font(36)).ColorAndOpacity(Accent)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,18)[SNew(STextBlock).Text(FText::FromString(TEXT("游戏结束"))).Font(Font(18)).ColorAndOpacity(Accent)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SButton).OnClicked_Lambda([this]{ if(Controller.IsValid()) { Controller->GetSession()->ResetSession(); UGameplayStatics::OpenLevel(Controller.Get(),TEXT("/HorrorSystems/Maps/Test_A")); } return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("重新开始"))).Font(Font(16))]]
            ]
        ]
        +SOverlay::Slot()
        [SNew(SBorder).Visibility_Lambda([this]{return EndTime()>0 && EndTime()<1.5f?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp(1.f-EndTime()/1.5f,0.f,1.f));})]
    ];
}
bool SHSOverlay::CinematicPlaying() const
{ auto* D=Controller.IsValid()?AHSRoomDirector::Find(Controller->GetWorld()):nullptr; return D && D->Progress()->bCinematic; }
float SHSOverlay::EndTime() const
{ auto* D=Controller.IsValid()?AHSRoomDirector::Find(Controller->GetWorld()):nullptr; return D?D->EndingTime:0.f; }
EVisibility SHSOverlay::GameplayVisibility() const { return CinematicPlaying() || EndTime()>0?EVisibility::Collapsed:EVisibility::SelfHitTestInvisible; }
FText SHSOverlay::CountdownText() const
{
    auto* D=Controller.IsValid()?AHSRoomDirector::Find(Controller->GetWorld()):nullptr;
    const float Remaining=D?D->Progress()->GetRemaining():60.f;
    const int32 Seconds=FMath::CeilToInt(Remaining);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60));
}
FText SHSOverlay::ClueText() const
{
    auto* D=Controller.IsValid()?AHSRoomDirector::Find(Controller->GetWorld()):nullptr;
    if(!D || !D->Rules) return FText::GetEmpty();
    const auto* P=D->Progress(); const auto* R=D->Rules->RouteFor(P->Stage);
    FString Text=FString::Printf(TEXT("%s  ·  阶段 %d\n\n线索"),*D->Rules->RoomName.ToString(),P->Stage);
    if(R) for(FName C:R->RequiredClues)
    { const FText* Label=D->Rules->ClueLabels.Find(C); Text+=FString::Printf(TEXT("\n%s  %s"),P->HasClue(C)?TEXT("[已获得]"):TEXT("[未获得]"),Label?*Label->ToString():*C.ToString()); }
    return FText::FromString(Text);
}
UHSItemData* SHSOverlay::HoverItem() const
{
    auto* S=Controller.IsValid()?Controller->GetSession():nullptr;
    return S && S->Slots.IsValidIndex(HoveredSlot)?S->Slots[HoveredSlot].Item.Get():nullptr;
}
void SHSOverlay::Tick(const FGeometry& G,double T,float Dt)
{
    SCompoundWidget::Tick(G,T,Dt);
    FVector2D Local=G.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
    TooltipPosition=FVector2D(FMath::Clamp(Local.X+18.f,8.f,FMath::Max(8.f,G.GetLocalSize().X-308.f)), FMath::Clamp(Local.Y-155.f,8.f,FMath::Max(8.f,G.GetLocalSize().Y-155.f)));
    UHSItemData* Item=Controller.IsValid()?Controller->GetInspectionItem():nullptr;
    if(LastInspection!=Item)
    {
        LastInspection=Item;
        InspectionBrush.SetResourceObject(Item?(Item->InspectionImage?Item->InspectionImage.Get():Item->Icon.Get()):nullptr);
    }
}
FText SHSOverlay::CompassText() const
{
    if(!Controller.IsValid()) return FText::GetEmpty();
    float Yaw=FMath::Fmod(Controller->GetControlRotation().Yaw+360.f,360.f);
    const TCHAR* Direction=Yaw<45 || Yaw>=315 ? TEXT("N 北") : Yaw<135 ? TEXT("E 东") : Yaw<225 ? TEXT("S 南") : TEXT("W 西");
    return FText::FromString(FString::Printf(TEXT("N   ·   E   ·   S   ·   W     |     %s  %03d°"),Direction,FMath::RoundToInt(Yaw)%360));
}
FReply SHSOverlay::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(!Controller.IsValid() || E.IsRepeat()) return FReply::Unhandled();
    if(E.GetKey()==EKeys::Escape) { Controller->Escape(); return FReply::Handled(); }
    if(E.GetKey()==EKeys::R) { Controller->InspectSelected(); return FReply::Handled(); }
    if(E.GetKey()==EKeys::Tab) { Controller->ToggleHotbarMouse(); return FReply::Handled(); }
    const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,EKeys::Zero};
    for(int32 I=0;I<10;++I) if(E.GetKey()==Keys[I]) { Controller->SelectSlot(I); return FReply::Handled(); }
    return FReply::Unhandled();
}
