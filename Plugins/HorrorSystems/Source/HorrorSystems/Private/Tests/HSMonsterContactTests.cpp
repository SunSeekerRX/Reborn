#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSAI.h"
#include "HSWorldState.h"
#include "HSProgression.h"
#include "HSRoomActors.h"
#include "HSPlayerController.h"
#include "HSSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSMonsterContactTest,"HorrorSystems.Combat.SceneOneContact",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
namespace
{
class FHSWaitForContactNavigation : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; TFunction<void()> Ready; double Started=-1;
public:
    FHSWaitForContactNavigation(FAutomationTestBase* InTest,TFunction<void()> InReady):Test(InTest),Ready(MoveTemp(InReady)) {}
    bool Update() override
    {
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        if(!W) {Test->AddError(TEXT("No game world"));return true;}
        if(Started<0) Started=W->GetTimeSeconds();
        const double Waited=W->GetTimeSeconds()-Started;
        if(Waited<1.0 || (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W) && Waited<8.0)) return false;
        Ready(); return true;
    }
};
}
bool FHSMonsterContactTest::RunTest(const FString& Parameters)
{
    if(Parameters.IsEmpty()) { ADD_LATENT_AUTOMATION_COMMAND(FHSWaitForContactNavigation(this,[this](){ RunTest(TEXT("NavigationReady")); })); return true; }
    UWorld* W=nullptr;
    for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
    auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
    auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
    AHSMonster* Monster=nullptr;
    if(W) for(TActorIterator<AHSMonster> It(W);It;++It) if(It->bCanDamagePlayer && !It->bCinematicActor) {Monster=*It;break;}
    auto* Room=AHSRoomDirector::Find(W);
    if(!Player || !Monster || !Room) {AddError(TEXT("Run on Basic_roomA with configured monster"));return false;}
    Room->FinishIntro(); PC->SetGameplayLocked(false);
    auto* State=PC->GetSession(); auto* Progress=Room->Progress();
    TestEqual(TEXT("Starts with three lives"),State->Lives,3);
    TestFalse(TEXT("Safe room prevents damage"),Monster->TryContactPlayer(Player));
    TestEqual(TEXT("Safe room preserves all lives"),State->Lives,3);
    FHitResult Ground;
    FCollisionQueryParams GroundQuery; GroundQuery.AddIgnoredActor(Player); GroundQuery.AddIgnoredActor(Monster);
    TestTrue(TEXT("Contact test position has an actual floor"),W->LineTraceSingleByChannel(Ground,FVector(-850,500,Player->GetActorLocation().Z),FVector(-850,500,-400),ECC_Visibility,GroundQuery));
    const float Z=Ground.ImpactPoint.Z+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f;
    Player->SetActorLocation(FVector(-850,500,Z)); Room->Tick(.01f);
    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,Monster->GetActorLocation(),Player->GetActorLocation(),Monster);
    AddInfo(FString::Printf(TEXT("Contact navigation: floor=%s actor=%s monster=%s player=%s path=%d partial=%d building=%d"),*Ground.ImpactPoint.ToString(),*GetNameSafe(Ground.GetActor()),*Monster->GetActorLocation().ToString(),*Player->GetActorLocation().ToString(),Path && Path->IsValid(),Path && Path->IsPartial(),UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W)));
    TestTrue(TEXT("Navigable route from monster to player"),Path && Path->IsValid() && !Path->IsPartial());
    Monster->SetActorLocation(FVector(-850,620,Z));
    FHitResult Hit; Monster->SetActorLocation(FVector(-850,500,Z),true,&Hit);
    TestTrue(TEXT("Physical capsule contact blocks movement"),Hit.bBlockingHit);
    TestEqual(TEXT("Physical contact loses exactly one life"),State->Lives,2);
    TestEqual(TEXT("Five second stagger begins"),Monster->StaggerRemaining,5.f);
    TestFalse(TEXT("Continuous contact cannot double hit"),Monster->TryContactPlayer(Player));
    Player->GetCharacterMovement()->TickComponent(.02f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Player recoil moves away from monster"),Player->GetVelocity().Y<0.f);
    Player->SetActorLocation(FVector(-850,500,Z));
    Monster->Tick(1.f);
    auto* AI=Cast<AHSPursuitController>(Monster->GetController());
    TestNotNull(TEXT("Pursuit controller active"),AI);
    if(AI) {AI->RefreshTarget();AI->Tick(.1f);}
    TestEqual(TEXT("Stagger speed is extremely slow"),Monster->GetCharacterMovement()->MaxWalkSpeed,30.f);
    Monster->Tick(4.f);
    TestEqual(TEXT("Stagger expires after five seconds"),Monster->StaggerRemaining,0.f);
    if(AI)
    {
        const auto* S=GetDefault<UHSSettings>();
        Monster->SetMovementState(EHSMovementState::Slow); AI->Tick(1.f);
        TestEqual(TEXT("Slow state speed"),Monster->GetCharacterMovement()->MaxWalkSpeed,S->NearSpeed);
        Monster->SetMovementState(EHSMovementState::Normal); AI->Tick(1.f);
        TestEqual(TEXT("Normal state speed"),Monster->GetCharacterMovement()->MaxWalkSpeed,S->NormalSpeed);
        Monster->SetMovementState(EHSMovementState::Fast); AI->Tick(1.f);
        TestEqual(TEXT("Fast state speed"),Monster->GetCharacterMovement()->MaxWalkSpeed,S->FarSpeed);
        Monster->bAutomaticSpeed=true;
    }
    TestFalse(TEXT("Still touching does not immediately rearm"),Monster->TryContactPlayer(Player));
    for(int32 Expected=1;Expected>=0;--Expected)
    {
        Monster->SetActorLocation(FVector(-850,800,Z)); Monster->Tick(6.f); Player->HitProtectionRemaining=0.f;
        Monster->SetActorLocation(FVector(-850,620,Z));
        Monster->SetActorLocation(FVector(-850,500,Z),true,&Hit);
        TestEqual(TEXT("Next separated contact loses one life"),State->Lives,Expected);
    }
    TestTrue(TEXT("Zero lives marks defeat"),State->IsDefeated());
    TestTrue(TEXT("Defeat locks player control"),PC->IsGameplayLocked());
    const float Time=Progress->GetRemaining(); Room->Tick(2.f);
    TestEqual(TEXT("Defeat freezes countdown"),Progress->GetRemaining(),Time);
    TestFalse(TEXT("Lives cannot go negative"),State->LoseLife());
    State->ResetSession(); TestEqual(TEXT("Restart restores three lives"),State->Lives,3);
    return true;
}
namespace
{
class FHSAutonomousContact : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHSMonster> Monster;
    TWeakObjectPtr<AHSCharacter> Player;
    double Started=-1;
public:
    explicit FHSAutonomousContact(FAutomationTestBase* InTest):Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        if(!W) {Test->AddError(TEXT("No game world"));return true;}
        auto* State=W->GetGameInstance()->GetSubsystem<UHSWorldState>();
        if(Started<0)
        {
            Player=Cast<AHSCharacter>(UGameplayStatics::GetPlayerPawn(W,0));
            for(TActorIterator<AHSMonster> It(W);It;++It) if(It->bCanDamagePlayer && !It->bCinematicActor) {Monster=*It;break;}
            if(!Player.IsValid() || !Monster.IsValid()) {Test->AddError(TEXT("SceneOne monster missing"));return true;}
            if(auto* Room=AHSRoomDirector::Find(W)) Room->FinishIntro();
            Player->SetActorLocation(FVector(-850,500,Player->GetActorLocation().Z));
            Started=W->GetTimeSeconds();
        }
        if(State->Lives<3)
        {
            Test->TestEqual(TEXT("Autonomous pursuit reaches and damages player"),State->Lives,2);
            Test->TestTrue(TEXT("Autonomous hit staggers monster"),Monster->StaggerRemaining>0.f);
            return true;
        }
        if(W->GetTimeSeconds()-Started>12.f) {
            auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
            FHitResult Obstacle;
            FCollisionQueryParams Query; Query.AddIgnoredActor(Monster.Get());
            W->SweepSingleByChannel(Obstacle,Monster->GetActorLocation(),Monster->GetActorLocation()+(Player->GetActorLocation()-Monster->GetActorLocation()).GetSafeNormal2D()*100.f,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38.f,90.f),Query);
            Test->AddInfo(FString::Printf(TEXT("Immediate obstacle actor=%s component=%s hit=%s"),*GetNameSafe(Obstacle.GetActor()),*GetNameSafe(Obstacle.GetComponent()),*Obstacle.ImpactPoint.ToString()));
            Test->AddError(FString::Printf(TEXT("Monster failed contact: monster=%s player=%s velocity=%s pursuit=%d timer=%d stagger=%.2f"),*Monster->GetActorLocation().ToString(),*Player->GetActorLocation().ToString(),*Monster->GetVelocity().ToString(),int32(Monster->PursuitState),P->bTimerRunning,Monster->StaggerRemaining));return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSAutonomousContactTest,"HorrorSystems.Combat.SceneOnePursuit",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSAutonomousContactTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FHSAutonomousContact(this)); return true; }
#endif
