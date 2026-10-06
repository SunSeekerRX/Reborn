#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HSSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Reborn Horror Systems"))
class HORRORSYSTEMS_API UHSSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UHSSettings() { CategoryName=TEXT("Game"); }
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> WalkFootstepSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> RunFootstepSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> LandingSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> CollapseSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> PaintingSwapSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> MonsterFootstepSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> AmbientBackgroundSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> Stage2Music;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> PlayerPressureSound;
    UPROPERTY(Config,EditAnywhere,Category="Audio") TSoftObjectPtr<class USoundBase> MonsterEntranceSound;
    UPROPERTY(Config,EditAnywhere,Category="Art") TSoftObjectPtr<class USkeletalMesh> MonsterAppearance;
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1")) float WalkSpeed=420.f;
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1")) float SprintSpeed=650.f;
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1")) float SlowSpeed=150.f;
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1")) float CrouchSpeed=230.f;
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.05", ClampMax="1")) float CtrlTapSeconds=.22f;
    UPROPERTY(Config, EditAnywhere, Category="Interaction", meta=(ClampMin="50")) float PickupDistance=280.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="40",ClampMax="100")) float CameraMinFOV=60.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="40",ClampMax="110")) float CameraMaxFOV=95.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1")) float CameraZoomStep=5.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1")) float CameraZoomSpeed=12.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.1",ClampMax="3")) float MouseSensitivity=.65f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="50",ClampMax="100")) float CameraFOV=85.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="10",ClampMax="85")) float StandingEyeOffset=68.f;
    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="10",ClampMax="52")) float CrouchedEyeOffset=50.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="1")) float NearSpeed=160.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="1")) float FarSpeed=540.f;
    UPROPERTY(Config,EditAnywhere,Category="Pursuit",meta=(ClampMin="1")) float NormalSpeed=300.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="100")) float NearRadius=800.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="100")) float FarRadius=1800.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="80")) float StopRadius=125.f;
    UPROPERTY(Config, EditAnywhere, Category="Pursuit", meta=(ClampMin="90")) float ResumeRadius=180.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="200")) float ClearRadius=500.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="300")) float HiddenRadius=1600.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="0.001", ClampMax="1")) float FogDensity=.08f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="100")) float FlashlightRange=800.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="100")) float PlayerLightRadius=330.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="1",Units="lm")) float PlayerLightIntensity=180.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="0")) float FlashlightIntensity=180.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="0",Units="lm")) float MonsterRedLightIntensity=65.f;
    UPROPERTY(Config, EditAnywhere, Category="Vision", meta=(ClampMin="100")) float MonsterRedLightRadius=280.f;
};
