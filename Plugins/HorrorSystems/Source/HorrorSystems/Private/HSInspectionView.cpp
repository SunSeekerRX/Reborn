#include "HSInspectionView.h"
#include "HSItemData.h"
#include "PreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

SHSInspectionView::~SHSInspectionView() { Brush.SetResourceObject(nullptr); Scene.Reset(); }
void SHSInspectionView::Construct(const FArguments&)
{
    Brush.DrawAs=ESlateBrushDrawType::Image;
    Brush.ImageSize=FVector2D(1100,560);
    ChildSlot[SNew(SImage).Image(&Brush)];
}
void SHSInspectionView::SetItem(UHSItemData* Item)
{
    if(Item==CurrentItem.Get() && (Item || !Scene)) return;
    bDragging=false; Brush.SetResourceObject(nullptr); Scene.Reset(); DisplayMaterial.Reset(); Target.Reset();
    Mesh=nullptr; Capture=nullptr; CurrentItem=Item;
    if(!Item) return;
    auto* Model=Item->InspectionMesh.Get();
    if(!Model) Model=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetEditor(false).SetCreatePhysicsScene(false).SetTransactional(false).SetForceMipsResident(false).SetSkyBrightness(0).SetLightBrightness(4));
    Mesh=NewObject<UStaticMeshComponent>(); Mesh->SetStaticMesh(Model);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for(int32 I=0;I<Item->InspectionMaterials.Num();++I) if(Item->InspectionMaterials[I]) Mesh->SetMaterial(I,Item->InspectionMaterials[I]);
    Scene->AddComponent(Mesh,FTransform::Identity);
    auto* Fill=NewObject<UDirectionalLightComponent>(); Fill->SetIntensity(2.f);
    Fill->SetLightColor(FLinearColor(.7f,.8f,1.f));
    Scene->AddComponent(Fill,FTransform(FRotator(-15,130,0)));
    Target.Reset(NewObject<UTextureRenderTarget2D>());
    Target->ClearColor=FLinearColor(0,0,0,1);
    Target->RenderTargetFormat=RTF_RGBA16f; Target->InitAutoFormat(1100,560);
    Capture=NewObject<USceneCaptureComponent2D>();
    Capture->TextureTarget=Target.Get(); Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->FOVAngle=38.f;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetAntiAliasing(false);
    Capture->ShowFlags.SetAmbientOcclusion(false);
    Capture->ShowFlags.SetGlobalIllumination(false);
    Capture->ShowFlags.SetScreenSpaceReflections(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true;
    Capture->PostProcessSettings.AutoExposureBias=0.f;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    Scene->AddComponent(Capture,FTransform::Identity);
    Scale=Item->InspectionScale.GetAbs().ComponentMax(FVector(.001f));
    Center=Model->GetBounds().Origin*Scale;
    Radius=(Model->GetBounds().BoxExtent*Scale).Size();
    Distance=FMath::Max(10.f,Radius/FMath::Tan(FMath::DegreesToRadians(19.f))*1.9643f*1.15f);
    Capture->SetWorldLocationAndRotation(FVector(Distance,0,0),FRotator(0,180,0));
    auto* BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/HorrorSystems/Items/Inspection/M_InspectionPreviewUI.M_InspectionPreviewUI"));
    if(BaseMaterial)
    {
        DisplayMaterial.Reset(UMaterialInstanceDynamic::Create(BaseMaterial,GetTransientPackage()));
        DisplayMaterial->SetTextureParameterValue(TEXT("PreviewTexture"),Target.Get());
        Brush.SetResourceObject(DisplayMaterial.Get());
    }
    WarmupFrames=8; ResetView();
}
void SHSInspectionView::ResetView()
{ Rotation=CurrentItem.IsValid()?CurrentItem->InspectionRotation.Quaternion():FQuat::Identity; Zoom=1.f; UpdateModel(); }
void SHSInspectionView::RotateModel(FVector2D Delta)
{
    Rotation=FQuat(FVector::UpVector,-Delta.X*.01f)*FQuat(FVector::RightVector,Delta.Y*.01f)*Rotation;
    Rotation.Normalize(); UpdateModel();
}
void SHSInspectionView::ZoomModel(float WheelDelta)
{ Zoom=FMath::Clamp(Zoom*FMath::Pow(1.12f,WheelDelta),.6f,2.5f); UpdateModel(); }
void SHSInspectionView::UpdateModel()
{
    if(!Mesh || !Capture) return;
    Mesh->SetWorldTransform(FTransform(Rotation,-Rotation.RotateVector(Center),Scale));
    Capture->SetWorldLocation(FVector(Distance/Zoom,0,0)); bDirty=true;
}
void SHSInspectionView::Tick(const FGeometry& G,double Time,float Dt)
{
    SCompoundWidget::Tick(G,Time,Dt);
    if(Target.IsValid() && G.GetLocalSize().X>1 && G.GetLocalSize().Y>1)
    {
        const int32 W=FMath::Clamp(FMath::RoundToInt(G.GetLocalSize().X),128,2048);
        const int32 H=FMath::Clamp(FMath::RoundToInt(G.GetLocalSize().Y),128,1200);
        if(Target->SizeX!=W || Target->SizeY!=H)
        {
            Target->ResizeTarget(W,H);
            Distance=FMath::Max(10.f,Radius/FMath::Tan(FMath::DegreesToRadians(19.f))*FMath::Max(1.f,float(W)/H)*1.15f);
            UpdateModel(); WarmupFrames=8;
        }
    }
    if(Capture && (bDirty || WarmupFrames>0))
    { Capture->CaptureScene(); bDirty=false; WarmupFrames=FMath::Max(0,WarmupFrames-1); }
}
FReply SHSInspectionView::OnMouseButtonDown(const FGeometry&,const FPointerEvent& E)
{ if(E.GetEffectingButton()==EKeys::LeftMouseButton && Mesh) { bDragging=true; return FReply::Handled().CaptureMouse(SharedThis(this)); } return FReply::Unhandled(); }
FReply SHSInspectionView::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)
{ if(E.GetEffectingButton()==EKeys::LeftMouseButton) { bDragging=false; return FReply::Handled().ReleaseMouseCapture(); } return FReply::Unhandled(); }
FReply SHSInspectionView::OnMouseMove(const FGeometry&,const FPointerEvent& E)
{ if(bDragging && HasMouseCapture()) { RotateModel(E.GetCursorDelta()); return FReply::Handled(); } return FReply::Unhandled(); }
FReply SHSInspectionView::OnMouseWheel(const FGeometry&,const FPointerEvent& E)
{ if(Mesh) { ZoomModel(E.GetWheelDelta()); return FReply::Handled(); } return FReply::Unhandled(); }
FReply SHSInspectionView::OnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent& E)
{ if(E.GetEffectingButton()==EKeys::LeftMouseButton) {ResetView();return FReply::Handled();} return FReply::Unhandled(); }
