#pragma once
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

class SHSParticleTitle : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHSParticleTitle) {} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        SetCanTick(true);
        for(int32 I=0;I<240;++I) for(int32 J=0;J<7;++J)
        {
            const float T=I*2.f*PI/240.f,V=(J-3)*.13f;
            const float R=1.f+V*FMath::Cos(T*.5f);
            const FVector P(R*FMath::Cos(T),R*FMath::Sin(T),V*FMath::Sin(T*.5f));
            Dots.Add(FVector2D(320+P.X*265,185+P.Y*95+P.Z*215));
        }
        const TCHAR* Glyphs[]={TEXT("11110/10001/10001/11110/10100/10010/10001"),TEXT("11111/10000/10000/11110/10000/10000/11111"),TEXT("11110/10001/10001/11110/10001/10001/11110"),TEXT("01110/10001/10001/10001/10001/10001/01110"),TEXT("11110/10001/10001/11110/10100/10010/10001"),TEXT("10001/11001/11001/10101/10011/10011/10001")};
        for(int32 Letter=0;Letter<6;++Letter)
        {
            const FString Glyph(Glyphs[Letter]);
            for(int32 Y=0;Y<7;++Y) for(int32 X=0;X<5;++X) if(Glyph[Y*6+X]=='1')
                for(int32 Dot=0;Dot<4;++Dot) Dots.Add(FVector2D(107+Letter*72+X*12+(Dot%2)*5,335+Y*12+(Dot/2)*5));
        }
    }
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D(640,440);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const FVector2D Scale=G.GetLocalSize()/FVector2D(640,440);
        for(int32 I=0;I<Dots.Num();++I)
        {
            const float Pulse=.65f+.35f*FMath::Sin(Time*1.7f+I*.19f);
            const FVector2D Drift(FMath::Sin(Time*.7f+I*1.41f)*.7f,FMath::Cos(Time*.6f+I*.53f)*.7f);
            const FVector2D Position=(Dots[I]+Drift)*Scale;
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(2.2f,2.2f)*Scale,FSlateLayoutTransform(Position)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(.73f,.9f,.79f,Pulse));
        }
        return Layer;
    }
    virtual void Tick(const FGeometry&,double T,float) override {Time=float(FMath::Fmod(T,10000.));}
private:
    TArray<FVector2D> Dots;
    float Time=0;
};

class SHSTransitionVeil : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHSTransitionVeil) : _Tint(FLinearColor::White) {} SLATE_ATTRIBUTE(float,Alpha) SLATE_ATTRIBUTE(bool,Death) SLATE_ATTRIBUTE(FLinearColor,Tint) SLATE_END_ARGS()
    void Construct(const FArguments& Args) {Alpha=Args._Alpha;Death=Args._Death;Tint=Args._Tint;}
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D::ZeroVector;}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const float A=Alpha.Get();if(A<=0) return Layer;
        const FVector2D Size=G.GetLocalSize();
        if(!Death.Get()) {FLinearColor Color=Tint.Get();Color.A=A;FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);return Layer;}
        const FVector2D Cover=Size*FMath::Min(1.f,A*1.5f),Center=Size*.5f;
        FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(Cover,FSlateLayoutTransform(Center-Cover*.5f)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(1,.93f,.93f,A));
        for(int32 I=0;I<24;++I)
        {
            const float Angle=I*2.f*PI/24.f;
            const FVector2D Direction(FMath::Cos(Angle),FMath::Sin(Angle));
            const float Length=FMath::Min(Size.X,Size.Y)*.4f*A;
            TArray<FVector2D> Points;
            for(int32 J=1;J<7;++J) Points.Add(Center+Direction*(Length*J/6.f)+FVector2D(-Direction.Y,Direction.X)*FMath::Sin(I*3.1f+J*1.9f)*12.f*A);
            FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(.65f,.01f,.025f,A*.72f),true,2.f+(I%3)*2.f);
        }
        return Layer+1;
    }
private:
    TAttribute<float> Alpha;
    TAttribute<bool> Death;
    TAttribute<FLinearColor> Tint;
};
