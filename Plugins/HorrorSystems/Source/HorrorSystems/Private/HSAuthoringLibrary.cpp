#include "HSAuthoringLibrary.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif
bool UHSAuthoringLibrary::CreatePickupAnimationAssets()
{
#if WITH_EDITOR
    const FString Folder=TEXT("/HorrorSystems/Characters/Interaction/");
    auto* Existing=LoadObject<UAnimMontage>(nullptr,*(Folder+TEXT("AM_FirstPersonPickup.AM_FirstPersonPickup")),nullptr,LOAD_NoWarn);
    if(Existing)
    {
        if(Existing->SlotAnimTracks.Num()>1)
        { FSlotAnimationTrack Valid=Existing->SlotAnimTracks.Last(); Existing->SlotAnimTracks.Reset(); Existing->SlotAnimTracks.Add(Valid); }
        if(Existing->CompositeSections.IsEmpty()) Existing->AddAnimCompositeSection(TEXT("Pickup"),0.f);
        Existing->PostEditChange(); FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Existing->GetOutermost(),Existing,*FPackageName::LongPackageNameToFilename(Existing->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save);
    }
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
    if(!Mesh || !Idle) return false;
    auto* Package=CreatePackage(*(Folder+TEXT("AS_FirstPersonPickup")));
    auto* Sequence=NewObject<UAnimSequence>(Package,TEXT("AS_FirstPersonPickup"),RF_Public|RF_Standalone);
    Sequence->SetSkeleton(Mesh->GetSkeleton());
    auto& Controller=Sequence->GetController(); Controller.InitializeModel();
    Controller.OpenBracket(FText::FromString(TEXT("First person pickup grip")),false);
    Controller.SetFrameRate(FFrameRate(30,1),false); Controller.SetNumberOfFrames(FFrameNumber(21),false);
    const auto& Skeleton=Mesh->GetRefSkeleton();
    const auto* IdleModel=Idle->GetDataModel();
    for(int32 Bone=0;Bone<Skeleton.GetNum();++Bone)
    {
        const FName Name=Skeleton.GetBoneName(Bone);
        FTransform Pose=Skeleton.GetRefBonePose()[Bone];
        if(IdleModel && IdleModel->IsValidBoneTrackName(Name))
        { TArray<FTransform> Poses; IdleModel->GetBoneTrackTransforms(Name,Poses); if(!Poses.IsEmpty()) Pose=Poses[0]; }
        TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
        for(int32 Frame=0;Frame<=21;++Frame)
        {
            const float Alpha=FMath::Sin(PI*Frame/21.f);
            FQuat Rotation=Pose.GetRotation();
            // Native component-space arm IK places the hand at the actual object.
            // The authored clip provides a wrist turn and finger grip, without root motion.
            const FString BoneName=Name.ToString();
            if(Name==TEXT("hand_r")) Rotation=Rotation*FQuat(FVector::ForwardVector,FMath::DegreesToRadians(12.f*Alpha));
            if(BoneName.EndsWith(TEXT("_r")) && (BoneName.StartsWith(TEXT("index_")) || BoneName.StartsWith(TEXT("middle_")) || BoneName.StartsWith(TEXT("ring_")) || BoneName.StartsWith(TEXT("pinky_"))))
                Rotation=Rotation*FQuat(FVector::RightVector,FMath::DegreesToRadians(25.f*Alpha));
            Positions.Add(FVector3f(Pose.GetTranslation())); Scales.Add(FVector3f(Pose.GetScale3D())); Rotations.Add(FQuat4f(Rotation.GetNormalized()));
        }
        Controller.AddBoneCurve(Name,false); Controller.SetBoneTrackKeys(Name,Positions,Rotations,Scales,false);
    }
    Controller.NotifyPopulated(); Controller.CloseBracket(false);
    Sequence->bEnableRootMotion=false; Sequence->PostEditChange(); FAssetRegistryModule::AssetCreated(Sequence);
    FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone;
    if(!UPackage::SavePackage(Package,Sequence,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Save)) return false;
    auto* MontagePackage=CreatePackage(*(Folder+TEXT("AM_FirstPersonPickup")));
    auto* Montage=NewObject<UAnimMontage>(MontagePackage,TEXT("AM_FirstPersonPickup"),RF_Public|RF_Standalone); Montage->SetSkeleton(Mesh->GetSkeleton());
    FSlotAnimationTrack Track; Track.SlotName=TEXT("DefaultSlot");
    FAnimSegment Segment; Segment.SetAnimReference(Sequence); Segment.StartPos=0; Segment.AnimStartTime=0; Segment.AnimEndTime=.7f; Segment.AnimPlayRate=1; Segment.LoopingCount=1;
    Track.AnimTrack.AnimSegments.Add(Segment); Montage->SlotAnimTracks.Reset(); Montage->SlotAnimTracks.Add(Track); Montage->SetCompositeLength(.7f); Montage->AddAnimCompositeSection(TEXT("Pickup"),0.f);
    Montage->BlendIn.SetBlendTime(.08f); Montage->BlendOut.SetBlendTime(.12f); Montage->PostEditChange(); FAssetRegistryModule::AssetCreated(Montage);
    return UPackage::SavePackage(MontagePackage,Montage,*FPackageName::LongPackageNameToFilename(MontagePackage->GetName(),FPackageName::GetAssetPackageExtension()),Save);
#else
    return false;
#endif
}
