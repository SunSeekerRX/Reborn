#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "HSWorldState.h"
#include "HSSceneInteractions.h"
#include "HSStoryActors.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FFinalEscape : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds();
    int32 Step=0,Point=0;
    TArray<FVector> Path;
    double LastProgress=FPlatformTime::Seconds();
    FVector LastPosition;
    bool LoadedEscapeMap=false;
public:
    explicit FFinalEscape(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>110) {Test->AddError(TEXT("Final escape run timed out"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
        if(!LoadedEscapeMap && W && PC)
        {
            PC->GetSession()->ResetSession();LoadedEscapeMap=true;
            UGameplayStatics::OpenLevel(W,TEXT("/HorrorSystems/Maps/Basic_roomABC_unchange1"));return false;
        }
        if(Step==2 && PC && PC->GetSession()->bTitleScreen && !PC->GetSession()->bWhiteTransition)
        {
            Test->TestTrue(TEXT("Completion returns to the original interactive title screen"),PC->bShowMouseCursor);
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/TitleAfterEscape.png")),true,false);
            return true;
        }
        if(!Player || !Room || FPlatformTime::Seconds()-Started<1) return false;
        // Directors activate C's exit blocker later in the run. Keep this geometry-only
        // fixture isolated each frame; Jam.FinalTableLure tests the live blocker and detour.
        for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;It->SetActorEnableCollision(false);}
        if(Step==0)
        {
            Room->Progress()->StoryStep=6;Room->Tick(0);
            Test->TestEqual(TEXT("ABC starts stage three"),Room->Progress()->Stage,3);
            Test->TestTrue(TEXT("ABC starts in A safety"),Room->IsSafe(Player));
            Test->TestTrue(TEXT("Single continuous escape director"),Room->bFinalEscapeMode);
            Room->FinishIntro();PC->SetGameplayLocked(false);
            AHSPortal* Exit=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules && It->RoomRules->RoomId==TEXT("RoomC") && It->bWhiteLightTravel) {Exit=*It;break;}
            if(!Exit) {Test->AddError(TEXT("Final exit missing"));return true;}
            Test->TestTrue(TEXT("Final exit locked without final information"),Exit->IsLocked());
            AHSPickup* Info=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("FinalMessage")) {Info=*It;break;}
            if(!Info) {Test->AddError(TEXT("Final information missing"));return true;}
            Player->SetActorLocation(Info->GetActorLocation()+FVector(-220,0,70));Room->Tick(.01f);
            Test->TestTrue(TEXT("Final information can be acquired"),Info->TryPickup(Player));
            Player->CancelAutoInspection();PC->CloseInspection();Room->Tick(.01f);
            Test->TestFalse(TEXT("Final information unlocks distant final exit"),Exit->IsLocked());
            Test->TestFalse(TEXT("Third stage does not seal connecting safety area"),Room->bSafeAreaSealed);
            int32 Collapses=0;
            for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It) if(It->bFalling) {It->Tick(2.f);Test->TestTrue(TEXT("Final A bookshelf falls"),It->bCollapsed);++Collapses;}
            Test->TestTrue(TEXT("Final A has a physical collapse"),Collapses>0);
            for(TActorIterator<AHSWoodDoor> It(W);It;++It)
            {It->Unlock();FVector Center,Extent;It->GetActorBounds(false,Center,Extent);Player->SetActorLocation(Center+FVector(0,100,0));if(!It->IsOpen()) It->Use(Player);It->Tick(1.f);}
            Player->SetActorLocation(Info->GetActorLocation()+FVector(-220,0,70));
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;It->SetActorEnableCollision(false);}
            // Find a physical capsule route through the authored connected mesh, without moving furniture.
            constexpr int32 Width=12,Height=106;TArray<FVector> Nodes;Nodes.SetNum(Width*Height);
            TArray<bool> Clear;Clear.Init(false,Nodes.Num());FCollisionQueryParams Params(SCENE_QUERY_STAT(HSFinalRun),false,Player);
            const auto Shape=FCollisionShape::MakeCapsule(34.f,88.f);
            for(int32 Y=0;Y<Height;++Y) for(int32 X=0;X<Width;++X)
            {
                const int32 I=Y*Width+X;const FVector Above(-1100+X*100,-9000+Y*100,550);
                FHitResult Floor;if(!W->LineTraceSingleByChannel(Floor,Above,Above-FVector(0,0,850),ECC_Pawn,Params) || Floor.Normal.Z<.7f) continue;
                // Lift the probe above stair risers; the real CharacterMovement run below verifies step handling.
                Nodes[I]=Floor.ImpactPoint+FVector(0,0,124);
                Clear[I]=!W->OverlapBlockingTestByChannel(Nodes[I],FQuat::Identity,ECC_Pawn,Shape,Params);
            }
            int32 Begin=INDEX_NONE,End=INDEX_NONE;float BeginD=MAX_flt,EndD=MAX_flt;
            for(int32 I=0;I<Nodes.Num();++I) if(Clear[I])
            {
                const float A=FVector::DistSquared2D(Nodes[I],Player->GetActorLocation()),B=FVector::DistSquared2D(Nodes[I],Exit->GetActorLocation());
                if(A<BeginD) {BeginD=A;Begin=I;} if(B<EndD) {EndD=B;End=I;}
            }
            if(Begin==INDEX_NONE || End==INDEX_NONE || EndD>FMath::Square(180.f)) {Test->AddError(TEXT("No reachable grid node near final exit"));return true;}
            TArray<int32> Previous;Previous.Init(INDEX_NONE,Nodes.Num());TArray<int32> Queue;Queue.Add(Begin);Previous[Begin]=Begin;
            for(int32 Q=0;Q<Queue.Num() && Previous[End]==INDEX_NONE;++Q)
            {
                const int32 I=Queue[Q],X=I%Width,Y=I/Width;
                for(const FIntPoint D:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
                {
                    const int32 NX=X+D.X,NY=Y+D.Y;if(NX<0 || NX>=Width || NY<0 || NY>=Height) continue;
                    const int32 N=NY*Width+NX;if(!Clear[N] || Previous[N]!=INDEX_NONE || FMath::Abs(Nodes[N].Z-Nodes[I].Z)>Player->GetCharacterMovement()->MaxStepHeight+3) continue;
                    FHitResult Hit;if(W->SweepSingleByChannel(Hit,Nodes[I],Nodes[N],FQuat::Identity,ECC_Pawn,Shape,Params)) continue;
                    Previous[N]=I;Queue.Add(N);
                }
            }
            FString Grid=TEXT("x,y,z,clear,reached\n");for(int32 I=0;I<Nodes.Num();++I) if(Clear[I]) Grid+=FString::Printf(TEXT("%.0f,%.0f,%.0f,1,%d\n"),Nodes[I].X,Nodes[I].Y,Nodes[I].Z,Previous[I]!=INDEX_NONE?1:0);
            FFileHelper::SaveStringToFile(Grid,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/FinalEscapeGrid.csv")));
            if(Previous[End]==INDEX_NONE) {Test->AddError(TEXT("Authored ABC geometry has no capsule route from final information to far exit"));return true;}
            for(int32 I=End;;I=Previous[I]) {Path.Insert(Nodes[I],0);if(I==Begin) break;}
            Path.Add(Exit->GetActorLocation());
            Test->AddInfo(FString::Printf(TEXT("Connected-map capsule route has %d waypoints"),Path.Num()));
            Step=1;LastProgress=FPlatformTime::Seconds();LastPosition=Player->GetActorLocation();return false;
        }
        if(Step==1)
        {
            if(Room->Progress()->bCompleted) {Step=2;return false;}
            if(Point>=Path.Num()) {Test->AddError(TEXT("Reached endpoint without triggering final white exit"));return true;}
            FVector Direction=Path[Point]-Player->GetActorLocation();Direction.Z=0;
            if(Direction.Size()<25) {++Point;return false;}
            if(FVector::Dist2D(LastPosition,Player->GetActorLocation())>20) {LastPosition=Player->GetActorLocation();LastProgress=FPlatformTime::Seconds();}
            if(FPlatformTime::Seconds()-LastProgress>4)
            {
                FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Player);W->SweepSingleByChannel(Hit,Player->GetActorLocation(),Player->GetActorLocation()+Direction.GetSafeNormal()*120,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,92),Q);
                Test->AddError(FString::Printf(TEXT("Walk blocked at point %d player=%s target=%s actor=%s impact=%s"),Point,*Player->GetActorLocation().ToString(),*Path[Point].ToString(),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString()));return true;
            }
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Pressed,1.f));Player->AddMovementInput(Direction.GetSafeNormal(),FMath::Clamp(Direction.Size()/120.f,.15f,1.f));
            PC->SetControlRotation(Direction.Rotation());return false;
        }
        Test->TestTrue(TEXT("Far exit completes the game"),Room->Progress()->bCompleted);
        Test->TestTrue(TEXT("Ending locks gameplay"),PC->IsGameplayLocked());
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSFinalEscapeTest,"HorrorSystems.Progression.FinalEscape",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSFinalEscapeTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FFinalEscape(this));return true;}
#endif
