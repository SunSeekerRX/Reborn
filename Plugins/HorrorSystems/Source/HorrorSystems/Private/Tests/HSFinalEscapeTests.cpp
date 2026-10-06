#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSRoomActors.h"
#include "HSWorldActors.h"
#include "HSProgression.h"
#include "HSAI.h"
#include "HSWorldState.h"
#include "HSSceneInteractions.h"
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
public:
    explicit FFinalEscape(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>70) {Test->AddError(TEXT("Final escape run timed out"));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE) W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=AHSRoomDirector::Find(W);
        if(Step==2 && PC && PC->GetSession()->bTitleScreen && !PC->GetSession()->bWhiteTransition)
        {
            Test->TestTrue(TEXT("Completion returns to the original interactive title screen"),PC->bShowMouseCursor);
            if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest"))) FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/TitleAfterEscape.png")),true,false);
            return true;
        }
        if(!Player || !Room || FPlatformTime::Seconds()-Started<1) return false;
        if(Step==0)
        {
            Test->TestEqual(TEXT("ABC starts stage three"),Room->Progress()->Stage,3);
            Test->TestTrue(TEXT("ABC starts in A safety"),Room->IsSafe(Player));
            Test->TestTrue(TEXT("Single continuous escape director"),Room->bFinalEscapeMode);
            Room->FinishIntro();PC->SetGameplayLocked(false);
            AHSPortal* Exit=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It) if(It->RoomRules==Room->Rules && It->bWhiteLightTravel) {Exit=*It;break;}
            if(!Exit) {Test->AddError(TEXT("Final exit missing"));return true;}
            Test->TestTrue(TEXT("Final exit locked without final information"),Exit->IsLocked());
            AHSPickup* Info=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It) if(It->ClueId==TEXT("Key_3")) {Info=*It;break;}
            if(!Info) {Test->AddError(TEXT("Final information missing"));return true;}
            Player->SetActorLocation(Info->GetActorLocation()+FVector(110,0,60));Room->Tick(.01f);
            Test->TestTrue(TEXT("Final information can be acquired"),Info->TryPickup(Player));
            Player->CancelAutoInspection();PC->CloseInspection();Room->Tick(.01f);
            Test->TestFalse(TEXT("Final information unlocks distant final exit"),Exit->IsLocked());
            Test->TestFalse(TEXT("Third stage does not seal connecting safety area"),Room->bSafeAreaSealed);
            int32 Collapses=0;
            TArray<AHSInspectTrigger*> Records;
            for(TActorIterator<AHSInspectTrigger> It(W);It;++It) if(It->Obstacle)
            {
                Player->SetActorLocation(It->GetActorLocation()+FVector(60,0,80));
                Test->TestTrue(TEXT("E collects furniture record"),It->Interact(Player));
                Test->TestFalse(TEXT("Collected record waits for inventory inspection"),PC->IsInspecting());
                Test->TestNotNull(TEXT("Collected record has independent inspection identity"),It->GetCollectedItem());
                for(auto* Other:Records) Test->TestTrue(TEXT("Shared source assets produce independent collected notes"),Other->GetCollectedItem()!=It->GetCollectedItem());
                Records.Add(*It);
            }
            for(auto* Record:Records)
            {
                const int32 Slot=PC->GetSession()->Slots.IndexOfByPredicate([&](const FHSItemSlot& S){return S.Item==Record->GetCollectedItem();});
                Test->TestTrue(TEXT("Collected record is in the hotbar"),Slot!=INDEX_NONE);
                PC->SelectSlot(Slot);PC->ActivateNumberSlot(Slot);PC->ActivateNumberSlot(Slot);
                Test->TestTrue(TEXT("Double number opens collected furniture record"),PC->IsInspecting());
                Test->TestTrue(TEXT("Inspection pauses world time"),UGameplayStatics::IsGamePaused(W));
                Test->TestFalse(TEXT("Furniture waits until inspection closes"),Record->Obstacle->bFalling);
                PC->CloseInspection();Record->Obstacle->Tick(2.f);
                Test->TestTrue(TEXT("Closing inspection triggers physical collapse"),Record->Obstacle->bCollapsed);++Collapses;
                for(auto* Other:Records) if(Other!=Record && !Other->Obstacle->bCollapsed) Test->TestFalse(TEXT("Other uninspected records do not collapse together"),Other->Obstacle->bFalling);
            }
            Test->TestTrue(TEXT("Third map contains inspection-triggered collapses"),Collapses>0);
            Player->SetActorLocation(Info->GetActorLocation()+FVector(110,0,60));
            for(TActorIterator<AHSRecoveryCheckpoint> It(W);It;++It) It->Tick(0);
            for(TActorIterator<AHSMonster> It(W);It;++It) {It->bCanDamagePlayer=false;It->bCinematicActor=true;}
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
                    const int32 N=NY*Width+NX;if(!Clear[N] || Previous[N]!=INDEX_NONE || FMath::Abs(Nodes[N].Z-Nodes[I].Z)>120) continue;
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
            Step=1;return false;
        }
        if(Step==1)
        {
            if(Room->Progress()->bCompleted) {Step=2;return false;}
            if(Point>=Path.Num()) {Test->AddError(TEXT("Reached endpoint without triggering final white exit"));return true;}
            FVector Direction=Path[Point]-Player->GetActorLocation();Direction.Z=0;
            if(Direction.Size()<25) {++Point;return false;}
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Pressed,1.f));Player->AddMovementInput(Direction.GetSafeNormal(),1.f);
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
