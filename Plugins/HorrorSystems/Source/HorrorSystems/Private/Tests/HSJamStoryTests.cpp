#include "Misc/AutomationTest.h"
#include "HSStoryActors.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldActors.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSSceneInteractions.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FJamStoryScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    bool bPhotoReturnOnly=false;
    int32 LastFocusedVisit=2;
    int32 Visit=0,Step=0;
    double Started=FPlatformTime::Seconds(),Until=Started+1;
    TWeakObjectPtr<UWorld> Departure;
    bool SawWhite=false;
    FVector SafePosition;
    FVector OriginalFurniture;
    FVector PhotoMonsterStart;
    TWeakObjectPtr<AHSPortal> Portal;
    TWeakObjectPtr<AHSPickup> Objective;
    FString PickupName;
    void Shot(const TCHAR* Name)
    {if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification"),Name),true,false);}
public:
    explicit FJamStoryScenario(FAutomationTestBase* T,bool Focused=false,int32 LastVisit=2):Test(T),bPhotoReturnOnly(Focused),LastFocusedVisit(LastVisit) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180) {Test->AddError(FString::Printf(TEXT("Story timed out at visit %d step %d"),Visit,Step));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        if(!PC) return false;
        auto* S=PC->GetSession();
        SawWhite|=S->bWhiteTransition && S->WhiteTravelAlpha>.999f;
        if(Step==9 && S->bTitleScreen && !S->bWhiteTransition)
        {Test->TestTrue(TEXT("Ending crosses an opaque white frame"),SawWhite);Test->AddInfo(TEXT("Completed all three stages and returned to title"));Shot(TEXT("JamEndingTitle.png"));return true;}
        auto* Room=Cast<AHSStoryDirector>(AHSRoomDirector::Find(W));
        if(Player && !Room && !S->bWhiteTransition && FPlatformTime::Seconds()>Until+3)
        {Test->AddError(TEXT("Loaded room is missing its story director; progression cannot advance"));return true;}
        if(!Player || !Room || S->bWhiteTransition || FPlatformTime::Seconds()<Until) return false;
        auto* P=Room->Progress();
        if(Step==0)
        {
            const FName Rooms[]={TEXT("RoomA"),TEXT("RoomB"),TEXT("RoomA"),TEXT("RoomC"),TEXT("RoomB"),TEXT("RoomA"),TEXT("RoomA")};
            Test->TestEqual(TEXT("Story room order"),Room->Rules->RoomId,Rooms[Visit]);
            Test->TestEqual(TEXT("Story node order"),P->StoryStep,Visit);
            Test->TestEqual(TEXT("Stage order"),P->Stage,Visit<2?1:Visit<6?2:3);
            if(bPhotoReturnOnly && Visit==2 && LastFocusedVisit==2)
            {Test->TestEqual(TEXT("First B return loads the actual A map"),UGameplayStatics::GetCurrentLevelName(W,true),FString(TEXT("Basic_roomA")));Test->TestTrue(TEXT("First B return arrives in A safety"),Room->IsSafe(Player));Shot(TEXT("FirstBReturnToA.png"));return true;}
            if(bPhotoReturnOnly && Visit==LastFocusedVisit && LastFocusedVisit>2)
            {Test->TestEqual(TEXT("Second-stage C then B returns to A"),UGameplayStatics::GetCurrentLevelName(W,true),FString(TEXT("Basic_roomA")));Test->TestTrue(TEXT("Second-stage return arrives in A safety"),Room->IsSafe(Player));Shot(TEXT("SecondStageReturnToA.png"));return true;}
            if(Visit==3) Test->TestEqual(TEXT("Second-stage A portal loads C, not B"),UGameplayStatics::GetCurrentLevelName(W,true),FString(TEXT("Basic_roomC")));
            if(Visit==4) Test->TestEqual(TEXT("Second-stage C portal loads B"),UGameplayStatics::GetCurrentLevelName(W,true),FString(TEXT("Basic_roomB")));
            Test->TestTrue(TEXT("Each arrival is inside safety"),Room->IsSafe(Player));
            Test->TestFalse(TEXT("Safe room countdown is stopped"),P->bTimerRunning);
            Test->TestTrue(TEXT("Player spawn has a walking floor"),Player->GetCharacterMovement()->IsMovingOnGround());
            Test->TestEqual(TEXT("New room grants sixty seconds"),P->GetRemaining(),60.f);
            Test->TestEqual(TEXT("Invisible barrier removed"),Room->ReturnBarrier->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
            Test->AddInfo(FString::Printf(TEXT("Jam arrival %d %s stage %d"),Visit,*W->GetOutermost()->GetName(),P->Stage));
            SafePosition=Player->GetActorLocation();
            if(!bPhotoReturnOnly && (Visit==0 || Visit==2 || Visit==5))
            {
                int32 Paintings=0;for(TActorIterator<AHSVisitPainting> It(W);It;++It) {++Paintings;Test->TestEqual(TEXT("A paintings change after first visit"),It->bBlank,Visit==0);}
                Test->TestEqual(TEXT("Both authored A paintings preserved"),Paintings,2);
                for(TActorIterator<AHSRoomVariation> It(W);It;++It)
                {
                    if(Visit==0)OriginalFurniture=It->Furniture->GetActorLocation();
                    else Test->TestTrue(TEXT("Revisited A physically moves its large furniture"),It->Furniture->GetActorLocation().Equals(OriginalFurniture+It->FurnitureOffset,1));
                    Test->TestEqual(TEXT("Blood writing appears on later visits"),It->Writing->IsVisible(),Visit!=0);
                }
            }
            if(Visit==0)
            {
                for(TActorIterator<AHSFlickerLamp> It(W);It;++It)
                {
                    Player->SetActorLocation(It->GetActorLocation()+FVector(650,0,-253));It->Tick(.01f);const int32 Count=It->EntryCount;
                    Player->SetActorLocation(It->GetActorLocation()+FVector(450,0,-253));It->Tick(.01f);Test->TestEqual(TEXT("Entry causes exactly one flicker event"),It->EntryCount,Count+1);
                    It->Tick(.01f);Test->TestEqual(TEXT("Remaining inside does not retrigger"),It->EntryCount,Count+1);
                    Player->SetActorLocation(It->GetActorLocation()+FVector(650,0,-253));It->Tick(.01f);Test->TestEqual(TEXT("Leaving does not flicker"),It->EntryCount,Count+1);
                    Player->SetActorLocation(It->GetActorLocation()+FVector(450,0,-253));It->Tick(.01f);Test->TestEqual(TEXT("Reentry flickers again"),It->EntryCount,Count+2);break;
                }
                Player->SetActorLocation(SafePosition);Room->Tick(0);
                Test->TestFalse(TEXT("Stage one A contains no active pursuer"),Room->Pursuer->bPursuitEnabled);
            }
            Portal=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules==Room->Rules) {Portal=*It;break;}
            if(Visit==6) {Step=7;return false;}
            if(!Portal.IsValid()) {Test->AddError(TEXT("Missing actual travel portal"));return true;}
            if(Visit==2) {Room->Tick(.01f);Step=4;return false;}
            Objective=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->RequiredStoryStep==Visit) {Objective=*It;break;}
            if(!Objective.IsValid()) {Test->AddError(TEXT("Missing design objective"));return true;}
            PickupName=Objective->ClueId.ToString();
            Player->SetActorLocation(FVector(-200,-780,SafePosition.Z));
            Test->TestTrue(TEXT("Wood door opens from inside safety"),Room->SafeDoor->Use(Player));Room->SafeDoor->Tick(1.f);
            Test->TestTrue(TEXT("Wood door rotates open"),Room->SafeDoor->IsOpen());
            Player->SetActorLocation(FVector(-200,-450,SafePosition.Z));Room->Tick(.01f);Room->SafeDoor->Tick(1.f);
            Test->TestTrue(TEXT("Exiting starts countdown"),P->bTimerRunning);
            Test->TestTrue(TEXT("Wood door locks on exit"),Room->SafeDoor->bLocked);
            Test->TestFalse(TEXT("Wood door closes on exit"),Room->SafeDoor->IsOpen());
            Test->TestFalse(TEXT("Cannot unlock without design clue"),Room->SafeDoor->Use(Player));
            Test->TestEqual(TEXT("Closed wood door physically blocks pawn"),Room->SafeDoor->PawnBlocker->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
            Test->TestTrue(TEXT("Portal is locked before objective"),Portal->IsLocked());Portal->Tick(0);Test->TestFalse(TEXT("Closed door conceals portal glow"),Portal->Marker->IsVisible());
            // Freeze damage during narrative traversal; pursuit state itself remains live.
            for(TActorIterator<AHSMonster> It(W);It;++It) It->bCanDamagePlayer=false;
            FVector PickupPosition=Objective->GetActorLocation()+FVector(100,0,70);
            if(bPhotoReturnOnly)
            {
                // Stand on connected walkable floor, not on the authored tabletop.
                auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
                FCollisionQueryParams Q;Q.AddIgnoredActor(Player);Q.AddIgnoredActor(Objective.Get());
                bool Found=false;
                for(int32 I=0;Nav && I<16 && !Found;++I)
                {
                    const float Angle=I*2.f*PI/16.f;FNavLocation Floor;
                    const FVector Candidate=Objective->GetActorLocation()+FVector(FMath::Cos(Angle)*170.f,FMath::Sin(Angle)*170.f,-100.f);
                    if(!Nav->ProjectPointToNavigation(Candidate,Floor,FVector(40,40,250))) continue;
                    const FVector Standing=Floor.Location+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
                    if(W->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q)) continue;
                    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,Room->Pursuer->GetActorLocation(),Floor.Location,Room->Pursuer);
                    if(!Path || !Path->IsValid() || Path->IsPartial()) continue;
                    PickupPosition=Standing;Found=true;
                }
                if(!Found) {Test->AddError(TEXT("No connected floor within pickup range of authored clue"));return true;}
            }
            Player->SetActorLocation(PickupPosition);Room->Tick(.01f);
            Test->TestTrue(TEXT("Actual authored objective can be picked up"),Objective->TryPickup(Player));Player->CancelAutoInspection();
            PC->ActivateNumberSlot(S->SelectedSlot);PC->ActivateNumberSlot(S->SelectedSlot);
            Test->TestTrue(TEXT("Double-number opens inspection"),PC->IsInspecting());
            if(!PC->GetInspectionItem()) {Test->AddError(TEXT("Inspection item was not selected"));return true;}
            Test->TestTrue(TEXT("Inspection pauses world time"),UGameplayStatics::IsGamePaused(W));
            Test->TestNotNull(TEXT("Clue uses a real 3D model"),PC->GetInspectionItem()->InspectionMesh.Get());
            Step=10;Until=FPlatformTime::Seconds()+.5;return false;
        }
        if(Step==10)
        {Shot(TEXT("JamClueInspection.png"));Step=11;Until=FPlatformTime::Seconds()+.2;return false;}
        if(Step==11)
        {
            PC->CloseInspection();Test->TestFalse(TEXT("Closing inspection resumes time"),UGameplayStatics::IsGamePaused(W));
            if(Visit==0)
            {
                Room->Tick(.01f);Test->TestTrue(TEXT("First photo starts Room A pursuit after inspection closes"),Room->Pursuer && Room->Pursuer->bPursuitEnabled);
                if(bPhotoReturnOnly && Room->Pursuer) {PhotoMonsterStart=Room->Pursuer->GetActorLocation();Step=12;Until=FPlatformTime::Seconds()+2;return false;}
            }
            if(Visit==1)
            {Test->TestTrue(TEXT("B window cinematic starts only after reading"),Room->WindowSequence && Room->WindowSequence->bPlaying);Test->TestFalse(TEXT("B chase remains hidden during cinematic"),Room->Pursuer->bPursuitEnabled);Step=1;return false;}
            Step=2;return false;
        }
        if(Step==12)
        {
            auto* AI=Cast<AHSPursuitController>(Room->Pursuer->GetController());
            auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);FNavLocation MonsterNav,PlayerNav;
            const bool MonsterOnNav=Nav && Nav->ProjectPointToNavigation(Room->Pursuer->GetActorLocation(),MonsterNav,FVector(100,100,300));
            const bool PlayerOnNav=Nav && Nav->ProjectPointToNavigation(Player->GetActorLocation(),PlayerNav,FVector(100,100,300));
            Test->AddInfo(FString::Printf(TEXT("Photo pursuit monster=%s player=%s velocity=%s controller=%s target=%s timer=%d mode=%d state=%d nav=%d/%d"),*Room->Pursuer->GetActorLocation().ToString(),*Player->GetActorLocation().ToString(),*Room->Pursuer->GetVelocity().ToString(),*GetNameSafe(AI),*GetNameSafe(AI?AI->Target.Get():nullptr),P->bTimerRunning,int32(Room->Pursuer->GetCharacterMovement()->MovementMode),int32(Room->Pursuer->PursuitState),MonsterOnNav,PlayerOnNav));
            Test->TestFalse(TEXT("Photo monster is rendered"),Room->Pursuer->IsHidden());
            Test->TestTrue(TEXT("Photo monster has collision"),Room->Pursuer->GetActorEnableCollision());
            Test->TestTrue(TEXT("Photo monster actually moves toward player on current room geometry"),FVector::DistSquared2D(PhotoMonsterStart,Room->Pursuer->GetActorLocation())>100.f);
            PC->SetControlRotation((Room->Pursuer->GetActorLocation()+FVector(0,0,65)-Player->GetActorLocation()-FVector(0,0,60)).Rotation());
            Shot(TEXT("PhotoMonsterActive.png"));Step=2;return false;
        }
        if(Step==1)
        {
            if(P->bCinematic) return false;Room->Tick(.01f);
            Test->TestTrue(TEXT("B chase starts after window performance"),Room->Pursuer->bPursuitEnabled);Shot(TEXT("JamAfterWindow.png"));Step=2;return false;
        }
        if(Step==2)
        {
            Room->Tick(.01f);Test->TestTrue(TEXT("Clue unlocks safe wood door"),Room->bObjectiveAcquired && !Room->SafeDoor->bLocked);
            Test->TestFalse(TEXT("Clue alone cannot teleport from outside safety"),Portal->Travel(Player));
            if(Visit==3)
            {
                const int32 Stage=P->Stage,Node=P->StoryStep;
                Room->Pursuer->bCanDamagePlayer=true;
                for(int32 Hit=0;Hit<3;++Hit) {Player->HitProtectionRemaining=0;Test->TestTrue(TEXT("Monster contact removes one life"),Player->ReceiveMonsterContact(Room->Pursuer));Test->TestEqual(TEXT("Three contacts reach zero before recovery"),S->Lives,2-Hit);}
                Test->TestTrue(TEXT("Death starts blood-white transition"),S->bDeathTransition && S->bWhiteTransition);
                Test->TestEqual(TEXT("Death retains stage"),P->Stage,Stage);Test->TestEqual(TEXT("Death retains story node"),P->StoryStep,Node);Step=3;return false;
            }
            Player->SetActorLocation(SafePosition);Room->Tick(.01f);Step=4;return false;
        }
        if(Step==3)
        {
            Test->TestEqual(TEXT("Recovery restores three lives"),S->Lives,3);
            Test->TestTrue(TEXT("Death returns to last safety"),Room->IsSafe(Player));
            Test->TestTrue(TEXT("Recovery retains acquired design clue"),P->HasClue(FName(*PickupName)));
            Test->TestFalse(TEXT("Recovery does not trap player behind locked wood door"),Room->SafeDoor->bLocked);Room->Tick(.01f);Step=4;return false;
        }
        if(Step==4)
        {
            Test->TestTrue(TEXT("Safe return authorizes portal"),Room->bSafeTravelReady);
            Test->TestFalse(TEXT("Safe return stops timer"),P->bTimerRunning);
            Test->TestFalse(TEXT("Ready portal is unlocked"),Portal->IsLocked());
            Departure=W;SawWhite=false;Test->TestTrue(TEXT("Actual portal begins white transfer"),Portal->Travel(Player));Step=5;return false;
        }
        if(Step==5 && Departure.Get()!=W)
        {Test->TestTrue(TEXT("Transfer is covered by opaque white"),SawWhite);++Visit;Step=0;Until=FPlatformTime::Seconds()+.8;return false;}
        if(Step==7)
        {
            AHSPickup* Final=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("FinalMessage")) Final=*It;
            if(!Final) {Test->AddError(TEXT("Final message missing"));return true;}
            Player->SetActorLocation(FVector(-200,-780,SafePosition.Z));
            Test->TestTrue(TEXT("Initial ABC door opens before final key"),Room->SafeDoor->Use(Player));Room->SafeDoor->Tick(1.f);
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;Test->TestFalse(TEXT("Third-stage ghost hidden before final item"),It->bPursuitEnabled);}
            Player->SetActorLocation(Final->GetActorLocation()+FVector(100,0,70));Room->Tick(.01f);
            Test->TestTrue(TEXT("Final message pickup succeeds"),Final->TryPickup(Player));Player->CancelAutoInspection();Room->Tick(.01f);
            Test->TestTrue(TEXT("Final message activates escape objective"),P->bFinalChaseStarted);
            Test->TestFalse(TEXT("Monster has a delayed entrance"),Room->Pursuer->bPursuitEnabled);
            Step=8;Until=FPlatformTime::Seconds()+3;return false;
        }
        if(Step==8)
        {
            Test->TestTrue(TEXT("Final chase starts after delay"),Room->Pursuer->bPursuitEnabled);
            if(auto* AI=Cast<AHSPursuitController>(Room->Pursuer->GetController()))
            {
                const FVector Home=Room->Pursuer->GetActorLocation(),Origin=Player->GetActorLocation();AI->RefreshTarget();
                Room->Pursuer->SetActorLocation(Origin+FVector(2500,0,0));AI->Tick(1.f);Test->TestEqual(TEXT("Monster catches up quickly outside visibility range"),Room->Pursuer->MovementState,EHSMovementState::Fast);
                Room->Pursuer->SetActorLocation(Origin+FVector(700,0,0));AI->Tick(1.f);Test->TestEqual(TEXT("Monster approaches normally inside range"),Room->Pursuer->MovementState,EHSMovementState::Normal);
                Room->Pursuer->SetActorLocation(Origin+FVector(300,0,0));AI->Tick(1.f);Test->TestEqual(TEXT("Monster uses slow state close to player"),Room->Pursuer->MovementState,EHSMovementState::Slow);
                Room->Pursuer->SetActorLocation(Home);
            }
            int32 Collapses=0;for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It) if(It->bCollapsed) {++Collapses;Test->TestEqual(TEXT("Collapsed shelf retains collision"),It->Mesh->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics);}
            Test->TestTrue(TEXT("Final A bookshelf actually falls"),Collapses>0);
            for(TActorIterator<AHSWoodDoor> It(W);It;++It)
            {
                FVector Center,Extent;It->GetActorBounds(false,Center,Extent);Player->SetActorLocation(Center+FVector(0,100,0));
                Test->TestTrue(TEXT("One final item opens every later door"),It->Use(Player));It->Tick(1.f);
            }
            // Visit B and C checkpoints in the same world, asserting independent room clocks.
            for(const FName Id:{FName(TEXT("RoomB")),FName(TEXT("RoomC"))})
            {
                AHSStoryDirector* Next=nullptr;for(TActorIterator<AHSStoryDirector> It(W);It;++It) if(It->Rules->RoomId==Id) Next=*It;
                Test->TestNotNull(TEXT("Connected room director exists"),Next);if(!Next) continue;
                if(Id==TEXT("RoomB"))
                {
                    const FTransform LastSafety=S->RecoveryTransform;
                    Player->SetActorLocation(FVector(-200,-1250,292));Next->Tick(.01f);
                    Test->TestFalse(TEXT("Crossing room boundary alone does not start its safe-room clock"),P->bTimerRunning);
                    Test->TestTrue(TEXT("Unvisited checkpoint cannot replace last recovery position"),S->RecoveryTransform.Equals(LastSafety));
                }
                Player->SetActorLocation(Next->SafeArea->GetComponentLocation()-FVector(0,0,18));Next->Tick(.01f);
                Test->TestFalse(TEXT("Connected checkpoint pauses countdown"),P->bTimerRunning);
                Player->SetActorLocation(Next->SafeArea->GetComponentLocation()+FVector(0,-350,-18));Next->Tick(.01f);
                Test->TestTrue(TEXT("Connected room starts its own countdown"),P->bTimerRunning);
                Test->TestTrue(TEXT("Connected room has a fresh sixty-second budget"),P->GetRemaining()>59.f);
                if(Id==TEXT("RoomB"))
                {Test->TestFalse(TEXT("B ghost waits until player passes its entrance"),Next->Pursuer->bPursuitEnabled);Player->SetActorLocation(Next->MonsterTriggerPoint-FVector(0,50,0));Next->Tick(.01f);Test->TestTrue(TEXT("B pursuer activates behind player"),Next->Pursuer->bPursuitEnabled && Next->Pursuer->GetActorLocation().Y>Player->GetActorLocation().Y);}
            }
            auto* C=Cast<AHSStoryDirector>(AHSRoomDirector::Find(W));
            Player->SetActorLocation(C->MonsterTriggerPoint);C->Tick(.01f);Test->TestTrue(TEXT("C monster appears at exit approach"),C->Pursuer->bPursuitEnabled);
            AHSPortal* Exit=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules==C->Rules) Exit=*It;
            if(!Exit) {Test->AddError(TEXT("Final exit missing"));return true;}
            Player->SetActorLocation(Exit->GetActorLocation()+FVector(0,100,42));C->Tick(.01f);
            SawWhite=false;const bool ExitAccepted=P->bCompleted || Exit->Travel(Player);
            Test->TestTrue(TEXT("Final exit accepts same final key"),ExitAccepted);Step=9;return false;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSJamStoryTest,"HorrorSystems.Jam.FullStory",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSJamStoryTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FJamStoryScenario(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSPhotoReturnTest,"HorrorSystems.Jam.FirstPhotoAndBReturn",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSPhotoReturnTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FJamStoryScenario(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSSecondStageRouteTest,"HorrorSystems.Jam.SecondStageACB",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSSecondStageRouteTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FJamStoryScenario(this,true,5));return true;}
#endif
