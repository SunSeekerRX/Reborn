#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSSettings.h"
#include "HSRoomActors.h"
#include "HSItemData.h"
#include "HSPolicy.h"
#include "HSAnimInstance.h"
#include "HSAI.h"
#include "HSProgression.h"
#include "HSSceneInteractions.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimMontage.h"

AHSCharacter::AHSCharacter()
{
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(38.f,92.f);
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
    GetCharacterMovement()->SetCrouchedHalfHeight(58.f);
    GetCharacterMovement()->bOrientRotationToMovement=false;
    GetCharacterMovement()->RotationRate=FRotator(0,540,0);
    GetCharacterMovement()->BrakingDecelerationWalking=1600;
    bUseControllerRotationYaw=true;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(12,0,68));
    Camera->bUsePawnControlRotation=true;
    Camera->FieldOfView=85.f;
    Flashlight=CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
    Flashlight->SetupAttachment(Camera);
    Flashlight->Intensity=180.f;
    Flashlight->InnerConeAngle=24.f; Flashlight->OuterConeAngle=48.f;
    Flashlight->SetLightColor(FLinearColor(.85f,.9f,1.f));
    PlayerAuraLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("PlayerAuraLight"));
    PlayerAuraLight->SetupAttachment(Camera);
    PlayerAuraLight->SetMobility(EComponentMobility::Movable);
    PlayerAuraLight->SetIntensityUnits(ELightUnits::Lumens);
    PlayerAuraLight->Intensity=180.f;
    PlayerAuraLight->AttenuationRadius=330.f;
    PlayerAuraLight->SourceRadius=12.f;
    PlayerAuraLight->SetLightColor(FLinearColor(.88f,.94f,1.f));
    PlayerAuraLight->CastShadows=true;
    PlayerAuraLight->VolumetricScatteringIntensity=.05f;
    GetMesh()->SetRelativeLocation(FVector(0,0,-92));
    GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}
bool AHSCharacter::ReceiveMonsterContact(AHSMonster* Monster)
{
    auto* PC=Cast<AHSPlayerController>(GetController());
    auto* State=GetGameInstance()->GetSubsystem<UHSWorldState>();
    auto* Progress=GetGameInstance()->GetSubsystem<UHSProgression>();
    auto* Room=AHSRoomDirector::Find(GetWorld());
    if(!Monster || !Monster->bCanDamagePlayer || Monster->bCinematicActor || HitProtectionRemaining>0.f || !PC || PC->IsGameplayLocked() || PC->IsInspecting() || Progress->bCinematic || Progress->bCompleted || (Room && State->IsInSafety(this)) || !State->LoseLife()) return false;
    HitProtectionRemaining=FMath::Max(5.25f,Monster->StaggerDuration+.25f);
    FVector Away=(GetActorLocation()-Monster->GetActorLocation()).GetSafeNormal2D();
    if(Away.IsNearlyZero()) Away=GetActorForwardVector();
    LaunchCharacter(Away*180.f+FVector(0,0,30),true,true);
    if(State->IsDefeated())
    {
          State->RecoverAtSafety();
    }
    return true;
}
void AHSCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (!GetMesh()->GetSkeletalMeshAsset())
        GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    if (!GetMesh()->GetAnimClass()) GetMesh()->SetAnimInstanceClass(UHSAnimInstance::StaticClass());
    // Full-body first person: keep the animated limbs, remove only neck/head.
    // The camera follows the capsule rather than head animation, preventing head bob.
    GetMesh()->HideBoneByName(TEXT("neck_01"),PBO_None);
    PickupHighlight=LoadObject<UMaterialInterface>(nullptr,TEXT("/HorrorSystems/Materials/M_HighlightPickup.M_HighlightPickup"));
    PaintingHighlight=LoadObject<UMaterialInterface>(nullptr,TEXT("/HorrorSystems/Materials/M_HighlightPainting.M_HighlightPainting"));
    const auto* Settings=GetDefault<UHSSettings>();
    Flashlight->AttenuationRadius=Settings->FlashlightRange;
    Flashlight->SetIntensity(Settings->FlashlightIntensity);
    PlayerAuraLight->SetAttenuationRadius(FMath::Clamp(Settings->PlayerLightRadius,100.f,FMath::Max(100.f,Settings->ClearRadius)));
    PlayerAuraLight->SetIntensity(FMath::Max(1.f,Settings->PlayerLightIntensity));
    PlayerAuraLight->SetVisibility(true);
    CameraFOVTarget=FMath::Clamp(Settings->CameraFOV,Settings->CameraMinFOV,FMath::Max(Settings->CameraMinFOV,Settings->CameraMaxFOV));
    Camera->FieldOfView=CameraFOVTarget;
    SmoothedEyeOffset=Settings->StandingEyeOffset;
    Camera->SetRelativeLocation(FVector(12,0,SmoothedEyeOffset));
    if(!PickupMontage) PickupMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/HorrorSystems/Characters/Interaction/AM_FirstPersonPickup.AM_FirstPersonPickup"));
    RefreshSpeed();
}
void AHSCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxisKey(EKeys::MouseX,this,&AHSCharacter::LookYaw);
    Input->BindAxisKey(EKeys::MouseY,this,&AHSCharacter::LookPitch);
    Input->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AHSCharacter::ZoomIn);
    Input->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AHSCharacter::ZoomOut);
    Input->BindKey(EKeys::LeftShift,IE_Pressed,this,&AHSCharacter::SprintPressed);
    Input->BindKey(EKeys::LeftShift,IE_Released,this,&AHSCharacter::SprintReleased);
    Input->BindKey(EKeys::LeftControl,IE_Pressed,this,&AHSCharacter::CtrlPressed);
    Input->BindKey(EKeys::LeftControl,IE_Released,this,&AHSCharacter::CtrlReleased);
    Input->BindKey(EKeys::E,IE_Pressed,this,&AHSCharacter::Interact);
}
void AHSCharacter::MoveForward(float Value)
{
    const APlayerController* PC=Cast<APlayerController>(Controller);
    if(!PC || PC->IsMoveInputIgnored()) return;
    AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),Value);
}
void AHSCharacter::MoveRight(float Value)
{
    const APlayerController* PC=Cast<APlayerController>(Controller);
    if(!PC || PC->IsMoveInputIgnored()) return;
    AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),Value);
}
void AHSCharacter::LookYaw(float V) { AddControllerYawInput(V*GetDefault<UHSSettings>()->MouseSensitivity); }
void AHSCharacter::LookPitch(float V) { AddControllerPitchInput(-V*GetDefault<UHSSettings>()->MouseSensitivity); }
void AHSCharacter::ZoomIn() { ChangeZoom(-1); }
void AHSCharacter::ZoomOut() { ChangeZoom(1); }
void AHSCharacter::ChangeZoom(float Direction)
{
    const auto* PC=Cast<AHSPlayerController>(Controller);
    if(!PC || PC->IsMouseMode() || PC->IsLookInputIgnored()) return;
    const auto* Settings=GetDefault<UHSSettings>();
    CameraFOVTarget=FMath::Clamp(CameraFOVTarget+Direction*Settings->CameraZoomStep,Settings->CameraMinFOV,FMath::Max(Settings->CameraMinFOV,Settings->CameraMaxFOV));
}
void AHSCharacter::OnStartCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust,ScaledHalfHeightAdjust);
    SmoothedEyeOffset+=HalfHeightAdjust;
}
void AHSCharacter::OnEndCrouch(float HalfHeightAdjust,float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust,ScaledHalfHeightAdjust);
    SmoothedEyeOffset-=HalfHeightAdjust;
}
void AHSCharacter::SprintPressed() { if(auto* PC=Cast<APlayerController>(Controller); PC && PC->IsMoveInputIgnored()) return; bSprintHeld=true; RefreshSpeed(); }
void AHSCharacter::SprintReleased() { bSprintHeld=false; RefreshSpeed(); }
void AHSCharacter::CtrlPressed() { if(auto* PC=Cast<APlayerController>(Controller); PC && PC->IsMoveInputIgnored()) return; bCtrlHeld=true; CtrlPressedAt=GetWorld()->GetTimeSeconds(); }
void AHSCharacter::CtrlReleased()
{
    if(bCtrlHeld && HSPolicy::IsCrouchTap(GetWorld()->GetTimeSeconds()-CtrlPressedAt,GetDefault<UHSSettings>()->CtrlTapSeconds))
    { if(bIsCrouched) UnCrouch(); else Crouch(); }
    bCtrlHeld=false; RefreshSpeed();
}
bool AHSCharacter::IsSlowWalking() const
{ return bCtrlHeld && !bIsCrouched && GetWorld()->GetTimeSeconds()-CtrlPressedAt>=GetDefault<UHSSettings>()->CtrlTapSeconds; }
void AHSCharacter::ClearMovementModifiers() { bSprintHeld=false; bCtrlHeld=false; GetCharacterMovement()->StopMovementImmediately(); RefreshSpeed(); }
void AHSCharacter::RefreshSpeed()
{
    const auto* S=GetDefault<UHSSettings>();
    GetCharacterMovement()->MaxWalkSpeed=HSPolicy::MoveSpeed(bIsCrouched,bSprintHeld,IsSlowWalking(),S->WalkSpeed,S->SprintSpeed,S->SlowSpeed,S->CrouchSpeed);
    GetCharacterMovement()->MaxWalkSpeedCrouched=S->CrouchSpeed;
}
void AHSCharacter::Tick(float Dt)
{
    HitProtectionRemaining=FMath::Max(0.f,HitProtectionRemaining-Dt);
    Super::Tick(Dt); RefreshSpeed(); RefreshFocus();
    const auto* Settings=GetDefault<UHSSettings>();
    Camera->FieldOfView=FMath::FInterpTo(Camera->FieldOfView,CameraFOVTarget,Dt,Settings->CameraZoomSpeed);
    const float EyeTarget=FMath::Clamp(bIsCrouched?Settings->CrouchedEyeOffset:Settings->StandingEyeOffset,10.f,GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-6.f);
    SmoothedEyeOffset=FMath::FInterpTo(SmoothedEyeOffset,EyeTarget,Dt,12.f);
    FVector EyePosition=GetActorTransform().TransformPosition(FVector(12,0,SmoothedEyeOffset));
    FHitResult EyeHit;
    FCollisionQueryParams EyeParams(SCENE_QUERY_STAT(HSFirstPersonEye),false,this);
    const FVector EyeStart=GetActorTransform().TransformPosition(FVector(12,0,0));
    if(GetWorld()->SweepSingleByChannel(EyeHit,EyeStart,EyePosition,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(4),EyeParams))
        EyePosition=EyeHit.Location;
    Camera->SetRelativeLocation(GetActorTransform().InverseTransformPosition(EyePosition));
    if(auto* PC=Cast<AHSPlayerController>(Controller); PC && !PC->IsMoveInputIgnored())
    {
        MoveForward((PC->IsInputKeyDown(EKeys::W)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::S)?1.f:0.f));
        MoveRight((PC->IsInputKeyDown(EKeys::D)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::A)?1.f:0.f));
    }
    const float PreviousPickupTime=PickupTimeLeft;
    PickupTimeLeft=FMath::Max(0.f,PickupTimeLeft-Dt);
    if(PreviousPickupTime>0 && PickupTimeLeft<=0 && PendingInspection)
    { auto* Item=PendingInspection.Get(); PendingInspection=nullptr; if(auto* PC=Cast<AHSPlayerController>(Controller)) PC->InspectItem(Item); }
    PickupPoseAlpha=PickupTimeLeft>0 ? FMath::Sin((1.f-PickupTimeLeft/PickupAnimationDuration)*PI) : 0;
    const float Speed=GetVelocity().Size2D();
    if(Speed>35 && !GetCharacterMovement()->IsFalling())
    {
        FootstepDistance+=Speed*Dt;
        if(FootstepDistance>(bIsCrouched?110.f:190.f))
        {
            FootstepDistance=0;
            auto* Sound=(bSprintHeld && !bIsCrouched?Settings->RunFootstepSound:Settings->WalkFootstepSound).LoadSynchronous();
            if(!Sound) Sound=FootstepSound;
            if(Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),bIsCrouched?.18f:.45f,1.f);
        }
    }
    else FootstepDistance=0;
}
void AHSCharacter::RefreshFocus()
{
    FocusedPickup=nullptr;
    FocusedInteraction=nullptr;
    auto* PC=Cast<AHSPlayerController>(Controller);
    if(!PC || PC->IsMouseMode() || PC->IsGameplayLocked() || PC->IsInspecting()) { ApplyInteractionHighlight(nullptr); return; }
    FVector Start; FRotator Rot; PC->GetPlayerViewPoint(Start,Rot);
    // Trace the precise view ray first: a wide sweep can hit a tabletop before a small key resting on it.
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HSPickupFocus),false,this);
    const float TraceDistance=GetDefault<UHSSettings>()->PickupDistance+80.f;
    const FVector End=Start+Rot.Vector()*TraceDistance;
    bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
    if(!bHit) bHit=GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(18),Params);
    if(bHit)
    {
        auto* Candidate=Hit.GetActor();
        if(Candidate && FVector::Dist(GetActorLocation(),Candidate->GetActorLocation())<=GetDefault<UHSSettings>()->PickupDistance && (Candidate->IsA<AHSPickup>() || Candidate->IsA<AHSSwapPainting>() || Candidate->IsA<AHSInspectTrigger>() || Candidate->IsA<AHSMovableProp>()))
        { FocusedInteraction=Candidate; FocusedPickup=Cast<AHSPickup>(Candidate); }
    }
    if(SelectedPainting.IsValid() && (SelectedPainting->bSwapping || FVector::Dist(GetActorLocation(),SelectedPainting->GetActorLocation())>GetDefault<UHSSettings>()->PickupDistance+50.f)) SelectedPainting.Reset();
    ApplyInteractionHighlight(FocusedInteraction?FocusedInteraction.Get():SelectedPainting.Get());
}
void AHSCharacter::ApplyInteractionHighlight(AActor* Target)
{
    auto* TargetMesh=Target?Target->FindComponentByClass<UMeshComponent>():nullptr;
    if(HighlightedMesh.Get()!=TargetMesh)
    { if(HighlightedMesh.IsValid()) {HighlightedMesh->SetOverlayMaterial(nullptr);HighlightedMesh->SetRenderCustomDepth(false);} HighlightedMesh=TargetMesh; }
    if(TargetMesh) {TargetMesh->SetOverlayMaterial(Target->IsA<AHSSwapPainting>()?PaintingHighlight:PickupHighlight);TargetMesh->SetCustomDepthStencilValue(Target->IsA<AHSSwapPainting>()?255:1);TargetMesh->SetRenderCustomDepth(true);}
}
void AHSCharacter::SelectPainting(AHSSwapPainting* Painting)
{
    if(!IsValid(Painting) || Painting->bSwapping) return;
    if(!SelectedPainting.IsValid()) SelectedPainting=Painting;
    else if(SelectedPainting.Get()!=Painting)
    {
        if(SelectedPainting->SwapWith(Painting)) SelectedPainting.Reset();
        else SelectedPainting=Painting;
    }
    ApplyInteractionHighlight(Painting);
}
void AHSCharacter::Landed(const FHitResult& Hit)
{ Super::Landed(Hit); if(auto* Sound=GetDefault<UHSSettings>()->LandingSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation()); }
void AHSCharacter::StartPickupAnimation(UHSItemData* Item,const FVector& Target)
{
    PickupTargetLocation=Target; PickupAnimationDuration=PickupMontage?FMath::Max(.1f,PickupMontage->GetPlayLength()):.7f; PickupTimeLeft=PickupAnimationDuration;
    if(PickupMontage) PlayAnimMontage(PickupMontage);
    // E stores the item; inventory inspection is explicitly requested afterwards.
    PendingInspection=nullptr;
}
void AHSCharacter::Interact()
{
    auto* PC=Cast<AHSPlayerController>(Controller);
    if(!PC || PC->IsMouseMode() || PC->IsGameplayLocked() || PickupTimeLeft>0) return;
    RefreshFocus();
    if(auto* Painting=Cast<AHSSwapPainting>(FocusedInteraction)) {SelectPainting(Painting);return;}
    if(auto* Trigger=Cast<AHSInspectTrigger>(FocusedInteraction)) {Trigger->Interact(this);return;}
    if(!FocusedPickup)
    {
        FVector Start; FRotator Rot; PC->GetPlayerViewPoint(Start,Rot); FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HSPropInteraction),false,this);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Rot.Vector()*GetDefault<UHSSettings>()->PickupDistance,ECC_Visibility,Params))
        { if(auto* Prop=Cast<AHSMovableProp>(Hit.GetActor())) Prop->Interact(this); else if(auto* Door=Cast<AHSPortal>(Hit.GetActor())) Door->Travel(this); }
        return;
    }
    if(FocusedPickup) PickupTargetLocation=FocusedPickup->GetActorLocation();
    if(FocusedPickup && FocusedPickup->TryPickup(this))
    {
        // TryPickup invokes StartPickupAnimation before the actor is destroyed.
    }
}

bool AHSCharacter::GetInteractionPromptLocation(FVector& Position) const
{
    const auto* PC=Cast<AHSPlayerController>(GetController());
    const auto* Target=FocusedInteraction.Get();
    if(!PC || PC->IsGameplayLocked() || PC->IsInspecting() || PC->IsMouseMode() || !IsValid(Target) || Target->IsHidden() || !Target->GetActorEnableCollision()) return false;
    if(FVector::Dist(GetActorLocation(),Target->GetActorLocation())>GetDefault<UHSSettings>()->PickupDistance) return false;
    if(const auto* Painting=Cast<AHSSwapPainting>(Target);Painting && Painting->bSwapping) return false;
    const auto* PromptMesh=Target->FindComponentByClass<UMeshComponent>();
    if(!PromptMesh || !PromptMesh->IsVisible()) return false;
    Position=PromptMesh->Bounds.Origin+FVector(0,0,PromptMesh->Bounds.BoxExtent.Z+12.f);return true;
}

