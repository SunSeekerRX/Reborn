#include "Misc/AutomationTest.h"
#include "HSAI.h"
#include "HSSettings.h"
#include "HSSceneInteractions.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSAnimInstance.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSArtTest,"HorrorSystems.Basic.ImportedArt",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSArtTest::RunTest(const FString&)
{
    UWorld* W=nullptr;for(auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
    AHSMonster* Monster=nullptr;AHSSceneAudio* Audio=nullptr;
    if(W) for(TActorIterator<AHSMonster> It(W);It;++It) if(!It->bCinematicActor) {Monster=*It;break;}
    if(W) for(TActorIterator<AHSSceneAudio> It(W);It;++It) {Audio=*It;break;}
    if(!Monster || !Audio) {AddError(TEXT("Run on a Basic scene with installed art"));return false;}
    if(auto* Room=AHSRoomDirector::Find(W)) Room->FinishIntro();
    auto* Visual=Monster->GetMesh();auto* Mesh=Visual->GetSkeletalMeshAsset();
    auto* Reference=LoadObject<USkeletalMesh>(nullptr,TEXT("/HorrorSystems/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    TestEqual(TEXT("Monster uses supplied butcher art"),GetNameSafe(Mesh),FString(TEXT("SKM_Butcher")));
    for(int32 I=0;I<Visual->GetNumMaterials();++I)
    {
        AddInfo(FString::Printf(TEXT("Butcher material %d: %s"),I,*GetNameSafe(Visual->GetMaterial(I))));
        TestTrue(TEXT("Supplied textured materials retained on runtime monster"),GetNameSafe(Visual->GetMaterial(I)).StartsWith(TEXT("M_Butcher_")));
    }
    TestTrue(TEXT("Butcher uses the existing locomotion skeleton"),Mesh && Mesh->GetSkeleton()==Reference->GetSkeleton());
    TestTrue(TEXT("Butcher height fits character capsule"),Visual->Bounds.BoxExtent.Z*2>130 && Visual->Bounds.BoxExtent.Z*2<260);
    TestNotNull(TEXT("Monster keeps original animation class"),Cast<UHSAnimInstance>(Visual->GetAnimInstance()));
    TestTrue(TEXT("Root, feet and head bones available"),Visual->GetBoneIndex(TEXT("root"))!=INDEX_NONE && Visual->GetBoneIndex(TEXT("foot_l"))!=INDEX_NONE && Visual->GetBoneIndex(TEXT("head"))!=INDEX_NONE);
    const FVector Before=Visual->GetBoneLocation(TEXT("foot_l"));
    Monster->GetCharacterMovement()->Velocity=FVector(300,0,0);Visual->TickAnimation(.3f,false);Visual->RefreshBoneTransforms();
    const FVector After=Visual->GetBoneLocation(TEXT("foot_l"));
    TestFalse(TEXT("Locomotion actually animates the bound skeleton"),Before.Equals(After,.01f));
    TestNotNull(TEXT("Background ambience bound"),Audio->Ambient->Sound.Get());
    TestNotNull(TEXT("Player footsteps bound"),GetDefault<UHSSettings>()->WalkFootstepSound.LoadSynchronous());
    TestNotNull(TEXT("Monster footsteps bound"),Monster->FootstepSound.Get());
    TestNotNull(TEXT("Stage music bound"),GetDefault<UHSSettings>()->Stage2Music.LoadSynchronous());
    TestNotNull(TEXT("Pressure audio bound"),Audio->Pressure->Sound.Get());
    return true;
}
#endif
