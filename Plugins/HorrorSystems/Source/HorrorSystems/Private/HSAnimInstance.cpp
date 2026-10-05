#include "HSAnimInstance.h"
#include "HSCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

struct FHSAnimProxy : FAnimInstanceProxy
{
    FAnimNode_BlendSpacePlayer_Standalone Player;
    FAnimNode_Slot Slot;
    float Speed=0, CrouchAlpha=0, ReachAlpha=0;
    FVector ReachTarget=FVector::ZeroVector;
    FHSAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void Initialize(UAnimInstance* Instance) override
    {
        auto* HS=CastChecked<UHSAnimInstance>(Instance);
        if(!HS->Locomotion) HS->Locomotion=LoadObject<UBlendSpace>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
        Player.SetBlendSpace(HS->Locomotion);
        Player.SetLoop(true);
        Slot.SlotName=TEXT("DefaultSlot"); Slot.bAlwaysUpdateSourcePose=true; Slot.Source.SetLinkNode(&Player);
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        const auto* Character=Cast<ACharacter>(Instance->TryGetPawnOwner());
        Speed=Character ? Character->GetVelocity().Size2D() : 0;
        CrouchAlpha=FMath::FInterpTo(CrouchAlpha,Character && Character->bIsCrouched ? 1.f : 0.f,Dt,10.f);
        const auto* HS=Cast<AHSCharacter>(Character);
        ReachAlpha=HS ? HS->PickupPoseAlpha : 0.f;
        if(HS) ReachTarget=HS->GetMesh()->GetComponentTransform().InverseTransformPosition(HS->PickupTargetLocation);
    }
    virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
    {
        Player.SetPosition(FVector(Speed,0,0));
        FAnimInstanceProxy::UpdateAnimationNode(Context);
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Slot.Evaluate_AnyThread(Output);
        const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
        auto Index=[&](FName Name)
        {
            const int32 MeshIndex=Bones.GetPoseBoneIndexForBoneName(Name);
            return MeshIndex==INDEX_NONE?FCompactPoseBoneIndex(INDEX_NONE):Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
        };
        FCSPose<FCompactPose> Pose;
        Pose.InitPose(Output.Pose);
        // Manny is already skinned. Solve in component space rather than assuming
        // mirrored local bone axes; keep each joint's original length and skin weights.
        auto Solve=[&](FName UpperName,FName LowerName,FName EndName,const FVector& Effector,const FVector& Pole)
        {
            const auto Upper=Index(UpperName),Lower=Index(LowerName),End=Index(EndName);
            if(Upper.GetInt()==INDEX_NONE || Lower.GetInt()==INDEX_NONE || End.GetInt()==INDEX_NONE) return;
            FTransform A=Pose.GetComponentSpaceTransform(Upper),B=Pose.GetComponentSpaceTransform(Lower),C=Pose.GetComponentSpaceTransform(End);
            const FQuat EndRotation=C.GetRotation();
            AnimationCore::SolveTwoBoneIK(A,B,C,Pole,Effector,false,1.,1.);
            C.SetRotation(EndRotation);
            TArray<FBoneTransform,TInlineAllocator<3>> Transforms;
            Transforms.Emplace(Upper,A); Transforms.Emplace(Lower,B); Transforms.Emplace(End,C);
            Pose.SafeSetCSBoneTransforms(Transforms);
        };
        const auto Pelvis=Index(TEXT("pelvis")),LeftFoot=Index(TEXT("foot_l")),RightFoot=Index(TEXT("foot_r"));
        if(CrouchAlpha>.001f && Pelvis.GetInt()!=INDEX_NONE && LeftFoot.GetInt()!=INDEX_NONE && RightFoot.GetInt()!=INDEX_NONE)
        {
            const FVector L=Pose.GetComponentSpaceTransform(LeftFoot).GetLocation(), R=Pose.GetComponentSpaceTransform(RightFoot).GetLocation();
            FTransform Hips=Pose.GetComponentSpaceTransform(Pelvis);
            Hips.AddToTranslation(FVector(0,-9,-38)*CrouchAlpha);
            const FBoneTransform HipsChange(Pelvis,Hips);
            Pose.SafeSetCSBoneTransforms(MakeArrayView(&HipsChange,1));
            Solve(TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),L,L+FVector(0,110,55));
            Solve(TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),R,R+FVector(0,110,55));
            const auto Spine=Index(TEXT("spine_01"));
            if(Spine.GetInt()!=INDEX_NONE)
            {
                FTransform Torso=Pose.GetComponentSpaceTransform(Spine);
                Torso.SetRotation((FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-10.f*CrouchAlpha))*Torso.GetRotation()).GetNormalized());
                const FBoneTransform Bend(Spine,Torso);
                Pose.SafeSetCSBoneTransforms(MakeArrayView(&Bend,1));
            }
        }
        const auto Shoulder=Index(TEXT("upperarm_r")),Hand=Index(TEXT("hand_r"));
        if(ReachAlpha>.001f && Shoulder.GetInt()!=INDEX_NONE && Hand.GetInt()!=INDEX_NONE)
        {
            const FVector Root=Pose.GetComponentSpaceTransform(Shoulder).GetLocation();
            const FVector Current=Pose.GetComponentSpaceTransform(Hand).GetLocation();
            const FVector Direction=(ReachTarget-Root).GetSafeNormal();
            const FVector Goal=Root+Direction*FMath::Min(FVector::Distance(ReachTarget,Root),55.f);
            Solve(TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),FMath::Lerp(Current,Goal,ReachAlpha),Root+FVector(-50,-20,-25));
        }
        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose),Output.Pose);
        return true;
    }
};
FAnimInstanceProxy* UHSAnimInstance::CreateAnimInstanceProxy() { return new FHSAnimProxy(this); }
void UHSAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
