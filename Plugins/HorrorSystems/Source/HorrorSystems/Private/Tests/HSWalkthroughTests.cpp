#include "Misc/AutomationTest.h"
#include "HSCharacter.h"
#include "HSPlayerController.h"
#include "HSWorldState.h"
#include "HSStoryActors.h"
#include "HSWorldActors.h"
#include "HSSceneInteractions.h"
#include "HSItemData.h"
#include "HSAI.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// Plan with actual floor/capsule collision, then follow using CharacterMovement.
// Doors are omitted from planning only: the walker must use E to open them in the live world.
class FRoomWalker
{
public:
    TArray<FVector> Path;
    int32 Point=0;
    FVector LastPosition;
    double LastProgress=0,DoorWait=0;
    bool Plan(UWorld* W,AHSCharacter* Player,FVector Goal,bool Combined,FAutomationTestBase* Test,AHSPickup* Clue=nullptr)
    {
        Path.Reset();Point=0;LastPosition=Player->GetActorLocation();LastProgress=FPlatformTime::Seconds();
        constexpr int32 Width=23;
        constexpr float Grid=50.f;
        const int32 Height=Combined?211:61;const float Bottom=Combined?-9000.f:-1200.f;
        TArray<FVector> Nodes;Nodes.SetNum(Width*Height);TArray<bool> Clear;Clear.Init(false,Nodes.Num());
        FCollisionQueryParams Q(SCENE_QUERY_STAT(JamWalkthrough),false,Player);
        for(TActorIterator<AHSWoodDoor> It(W);It;++It) Q.AddIgnoredActor(*It);
        for(TActorIterator<AHSPickup> It(W);It;++It) Q.AddIgnoredActor(*It);
        for(TActorIterator<AHSMonster> It(W);It;++It) Q.AddIgnoredActor(*It);
        const auto Capsule=FCollisionShape::MakeCapsule(34,88);
        for(int32 Y=0;Y<Height;++Y) for(int32 X=0;X<Width;++X)
        {
            const int32 I=Y*Width+X;FHitResult Floor;const FVector Above(-1100+X*Grid,Bottom+Y*Grid,550);
            if(!W->LineTraceSingleByChannel(Floor,Above,Above-FVector(0,0,850),ECC_Pawn,Q) || Floor.Normal.Z<.7f) continue;
            Nodes[I]=Floor.ImpactPoint+FVector(0,0,124);
            Clear[I]=!W->OverlapBlockingTestByChannel(Nodes[I],FQuat::Identity,ECC_Pawn,Capsule,Q);
        }
        int32 Start=INDEX_NONE;float Near=MAX_flt;
        for(int32 I=0;I<Nodes.Num();++I) if(Clear[I])
        {const float D=FVector::DistSquared2D(Nodes[I],Player->GetActorLocation());if(D<Near){Near=D;Start=I;}}
        if(Start==INDEX_NONE){Test->AddError(TEXT("No collision-free starting floor"));return false;}
        TArray<int32> Previous;Previous.Init(INDEX_NONE,Nodes.Num());TArray<int32> Queue;Queue.Add(Start);Previous[Start]=Start;
        for(int32 At=0;At<Queue.Num();++At)
        {
            const int32 I=Queue[At],X=I%Width,Y=I/Width;
            for(FIntPoint D:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
            {
                const int32 NX=X+D.X,NY=Y+D.Y;if(NX<0||NX>=Width||NY<0||NY>=Height) continue;
                const int32 N=NY*Width+NX;
                if(!Clear[N]||Previous[N]!=INDEX_NONE||Nodes[N].Z-Nodes[I].Z>Player->GetCharacterMovement()->MaxStepHeight+3||Nodes[I].Z-Nodes[N].Z>110) continue;
                FHitResult Hit;if(W->SweepSingleByChannel(Hit,Nodes[I],Nodes[N],FQuat::Identity,ECC_Pawn,Capsule,Q)) continue;
                Previous[N]=I;Queue.Add(N);
            }
        }
        int32 End=INDEX_NONE;Near=MAX_flt;
        for(int32 I:Queue)
        {
            if(Clue)
            {
                const FVector Standing=Nodes[I]-FVector(0,0,30),Eye=Nodes[I]+FVector(0,0,38);
                if(FVector::Dist(Standing,Clue->Mesh->Bounds.Origin)>270)continue;
                FHitResult ViewHit;FCollisionQueryParams ViewQ(SCENE_QUERY_STAT(WalkClueApproach),true,Player);
                for(TActorIterator<AHSMonster> It(W);It;++It)ViewQ.AddIgnoredActor(*It);
                if(!W->LineTraceSingleByChannel(ViewHit,Eye,Clue->Mesh->Bounds.Origin,ECC_Visibility,ViewQ)||ViewHit.GetActor()!=Clue)continue;
            }
            const float D=FVector::DistSquared2D(Nodes[I],Goal);if(D<Near){Near=D;End=I;}
        }
        if(End==INDEX_NONE||(!Clue&&Near>FMath::Square(160.f)))
        {Test->AddError(FString::Printf(TEXT("No reachable floor near goal %s (nearest %.1f cm)"),*Goal.ToString(),FMath::Sqrt(Near)));return false;}
        for(int32 I=End;;I=Previous[I]){Path.Insert(Nodes[I],0);if(I==Start)break;}
        // Keep the final target on its nearest reachable floor, never inside furniture.
        Test->AddInfo(FString::Printf(TEXT("Walk route: %s -> %s, %d points"),*Player->GetActorLocation().ToString(),*Path.Last().ToString(),Path.Num()));
        return true;
    }
    int32 Tick(AHSPlayerController* PC,AHSCharacter* Player,FAutomationTestBase* Test)
    {
        const double Now=FPlatformTime::Seconds();
        if(Now<DoorWait)return 0;
        if(Point>=Path.Num()){Player->GetCharacterMovement()->StopMovementImmediately();Player->ClearMovementModifiers();return 1;}
        FVector Direction=Path[Point]-Player->GetActorLocation();Direction.Z=0;
        if(Direction.Size()<25){++Point;return 0;}
        for(TActorIterator<AHSWoodDoor> It(Player->GetWorld());It;++It)
        {
            FVector Center,Extent;It->GetActorBounds(false,Center,Extent);
            if(!It->bLocked&&!It->IsOpen()&&FVector::Dist2D(Player->GetActorLocation(),Center)<220)
            {
                Player->GetCharacterMovement()->StopMovementImmediately();
                PC->SetControlRotation((It->Mesh->Bounds.Origin-Player->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
                Player->RefreshInteractionFocus();
                if(Player->FocusedInteraction==*It)
                {PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));DoorWait=Now+.7;LastProgress=Now;return 0;}
            }
        }
        if(FVector::Dist2D(LastPosition,Player->GetActorLocation())>20){LastPosition=Player->GetActorLocation();LastProgress=Now;}
        if(Now-LastProgress>4)
        {
            FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Player);
            Player->GetWorld()->SweepSingleByChannel(Hit,Player->GetActorLocation(),Player->GetActorLocation()+Direction.GetSafeNormal()*120,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q);
            Test->AddError(FString::Printf(TEXT("Physical walk blocked: player %s goal %s actor %s component %s impact %s"),*Player->GetActorLocation().ToString(),*Path[Point].ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString()));return -1;
        }
        PC->SetControlRotation(Direction.Rotation());PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Pressed,1));
        Player->AddMovementInput(Direction.GetSafeNormal(),FMath::Clamp(Direction.Size()/120.f,.15f,1.f));return 0;
    }
};

class FPhysicalStory : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    FRoomWalker Walker;
    int32 Visit=0,Step=0;
    double Started=FPlatformTime::Seconds(),Until=0;
    TWeakObjectPtr<AHSPickup> Pickup;
    UHSItemData* Item=nullptr;
    TWeakObjectPtr<AHSPortal> Portal;
    TWeakObjectPtr<UWorld> Departure;
    bool SawWhite=false,SawBlack=false;
    bool DeathShot=false;
    double PortalWait=0;
    bool Plan(UWorld* W,AHSCharacter* Player,FVector Target){return Walker.Plan(W,Player,Target,Visit==6,Test);}
    void Shot(const FString& Name){if(FParse::Param(FCommandLine::Get(),TEXT("HSVisualTest")))FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Verification/Walkthrough"),Name+TEXT(".png")),true,false);}
public:
    explicit FPhysicalStory(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>420){Test->AddError(FString::Printf(TEXT("Walkthrough timed out: visit %d step %d"),Visit,Step));return true;}
        UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::Game||Context.WorldType==EWorldType::PIE)W=Context.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;if(!PC)return false;
        auto* S=PC->GetSession();auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
        if(Step==22&&S->bDeathTransition&&S->WhiteTravelAlpha>.6f&&!DeathShot){Shot(TEXT("80_DeathBloodWhite"));DeathShot=true;}
        SawWhite|=S->WhiteTravelAlpha>.999f;SawBlack|=S->EndingBlackAmount>.99f;
        if(Step==0)
        {
            Test->TestTrue(TEXT("Game begins with title"),S->bTitleScreen);Shot(TEXT("00_Title"));
            Test->TestTrue(TEXT("Start button callback accepted"),S->BeginMenuTravel(true));Step=1;return false;
        }
        if(Step==90 && S->bTitleScreen&&!S->bWhiteTransition)
        {
            Test->TestTrue(TEXT("Ending reached opaque white"),SawWhite);Test->TestTrue(TEXT("Ending includes black frame"),SawBlack);Shot(TEXT("99_EndingTitle"));
            Test->TestTrue(TEXT("Replay begins a fresh session"),S->BeginMenuTravel(true));Step=91;return false;
        }
        auto* Player=Cast<AHSCharacter>(PC->GetPawn());auto* Room=Cast<AHSStoryDirector>(AHSRoomDirector::Find(W));
        if(!Player||!Room||S->bWhiteTransition)return false;
        if(Step==91){Test->TestEqual(TEXT("Replay resets stage"),P->Stage,1);Test->TestEqual(TEXT("Replay resets story"),P->StoryStep,0);Test->TestEqual(TEXT("Replay resets health"),S->Lives,3);Test->AddInfo(TEXT("Entire title-to-ending physical walkthrough and replay completed"));return true;}
        if(FPlatformTime::Seconds()<Until)return false;
        // This run isolates traversal and interactions. Autonomous combat is covered separately.
        for(TActorIterator<AHSMonster> It(W);It;++It){It->bCanDamagePlayer=false;It->SetActorEnableCollision(false);}
        if(Step==1)
        {
            const FName Expected[]={TEXT("RoomA"),TEXT("RoomB"),TEXT("RoomA"),TEXT("RoomC"),TEXT("RoomB"),TEXT("RoomA"),TEXT("RoomA")};
            Test->TestEqual(TEXT("Actual map arrival order"),Room->Rules->RoomId,Expected[Visit]);Test->TestEqual(TEXT("Actual story arrival order"),P->StoryStep,Visit);
            Test->TestTrue(TEXT("Arrival in safety"),Room->IsSafe(Player));Test->TestFalse(TEXT("Safety pauses timer"),P->bTimerRunning);
            Portal=nullptr;for(TActorIterator<AHSPortal> It(W);It;++It)if(It->RoomRules==Room->Rules){Portal=*It;break;}
            Pickup=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It)if(It->RequiredStoryStep==Visit){Pickup=*It;break;}
            if(!Pickup.IsValid()){Test->AddError(TEXT("Authored clue missing"));return true;}
            Item=Pickup->ItemData;Shot(FString::Printf(TEXT("%02d_Arrival"),Visit+1));
            if(!Walker.Plan(W,Player,Pickup->GetActorLocation()+FVector(-220,0,70),Visit==6,Test,Pickup.Get()))return true;Step=2;return false;
        }
        if(Step==2)
        {
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;if(!Result)return false;
            Test->TestTrue(TEXT("Leaving safety starts timer"),P->bTimerRunning);
            if(!Pickup.IsValid()){Test->AddError(TEXT("Clue disappeared before E pickup"));return true;}
            PC->SetControlRotation((Pickup->Mesh->Bounds.Origin-Player->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);Player->RefreshInteractionFocus();
            if(Player->FocusedPickup!=Pickup.Get())
            {
                FVector View;FRotator Rotation;PC->GetPlayerViewPoint(View,Rotation);
                for(bool Complex:{false,true})
                {FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ClueFocusDiagnosis),Complex,Player);W->LineTraceSingleByChannel(Hit,View,View+Rotation.Vector()*360,ECC_Visibility,Q);
                Test->AddInfo(FString::Printf(TEXT("Focus diagnosis complex=%d player=%s eye=%s target=%s pickup=%s hit=%s component=%s at=%s distance=%.1f"),Complex,*Player->GetActorLocation().ToString(),*View.ToString(),*Pickup->Mesh->Bounds.Origin.ToString(),*Pickup->GetActorLocation().ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),FVector::Dist(Player->GetActorLocation(),Pickup->GetActorLocation())));}
            }
            Test->TestEqual(TEXT("Real view ray selects authored clue"),Player->FocusedPickup.Get(),Pickup.Get());
            int32 Highlights=0;for(TActorIterator<AActor> It(W);It;++It)if(auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>();Mesh&&Mesh->bRenderCustomDepth)++Highlights;
            Test->TestEqual(TEXT("Only one interactable is highlighted"),Highlights,1);
            FVector Prompt;Test->TestTrue(TEXT("Clue has E prompt above mesh"),Player->GetInteractionPromptLocation(Prompt));Shot(FString::Printf(TEXT("%02d_CluePrompt"),Visit+1));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));Until=FPlatformTime::Seconds()+.15;Step=3;return false;
        }
        if(Step==3)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
            Test->TestEqual(TEXT("E puts item into selected slot"),S->GetSelectedItem(),Item);
            if(S->GetSelectedItem()!=Item)return true;
            const FKey Numbers[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,EKeys::Zero};
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Numbers[S->SelectedSlot],IE_Pressed,1));
            Until=FPlatformTime::Seconds()+.08;Step=31;return false;
        }
        if(Step>=31&&Step<=33)
        {
            const FKey Numbers[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,EKeys::Zero};
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Numbers[S->SelectedSlot],Step==32?IE_Pressed:IE_Released,Step==32?1:0));
            if(Step==33){Step=4;Until=FPlatformTime::Seconds()+.6;}else{++Step;Until=FPlatformTime::Seconds()+.06;}return false;
        }
        if(Step==4)
        {
            Test->TestTrue(TEXT("Real double-number input opens inspection"),PC->IsInspecting());Test->TestTrue(TEXT("Inspection pauses the game"),UGameplayStatics::IsGamePaused(W));
            Shot(FString::Printf(TEXT("%02d_Inspection"),Visit+1));Until=FPlatformTime::Seconds()+.2;Step=5;return false;
        }
        if(Step==5)
        {
            PC->CloseInspection();Test->TestFalse(TEXT("Closing resumes game"),UGameplayStatics::IsGamePaused(W));
            if(Visit==1){Test->TestTrue(TEXT("B starts window performance after reading"),P->bCinematic);Step=6;return false;}
            Step=7;return false;
        }
        if(Step==6)
        {
            if(P->bCinematic)return false;
            Test->TestTrue(TEXT("B pursuer appears after performance"),Room->Pursuer->bPursuitEnabled);Shot(TEXT("02_WindowEnd"));Step=7;return false;
        }
        if(Step==7)
        {
            if(Visit==6)
            {
                // Wait for collapse to complete before scanning the final route.
                Until=FPlatformTime::Seconds()+3;Step=20;return false;
            }
            Test->TestFalse(TEXT("Current clue unlocks physical return door"),Room->SafeDoor->bLocked);
            if(!Plan(W,Player,Room->SafeArea->GetComponentLocation()))return true;Step=8;return false;
        }
        if(Step==8)
        {
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;if(!Result)return false;
            Test->TestTrue(TEXT("Walked back into safety"),Room->IsSafe(Player));Test->TestFalse(TEXT("Safe return stops timer"),P->bTimerRunning);
            for(TActorIterator<AHSSceneAudio> It(W);It;++It)Test->TestFalse(TEXT("Safe return stops chase music"),It->Music->IsPlaying());
            if(!Portal.IsValid()){Test->AddError(TEXT("Safe transfer portal missing"));return true;}
            Test->TestFalse(TEXT("Safe portal is unlocked"),Portal->IsLocked());
            Departure=W;SawWhite=false;PortalWait=0;if(!Plan(W,Player,Portal->GetActorLocation()))return true;Step=9;return false;
        }
        if(Step==9)
        {
            if(Departure.Get()!=W){Test->TestTrue(TEXT("Physical portal crossing covered by white"),SawWhite);++Visit;Step=1;Until=FPlatformTime::Seconds()+.7;return false;}
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;
            if(Result>0)
            {
                if(PortalWait==0)PortalWait=FPlatformTime::Seconds();
                const FVector Direction=(Portal->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D();
                if(FPlatformTime::Seconds()-PortalWait>3)
                {
                    FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Player);W->SweepSingleByChannel(Hit,Player->GetActorLocation(),Player->GetActorLocation()+Direction*150,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q);
                    Shot(TEXT("PortalBlocked"));Test->AddError(FString::Printf(TEXT("Portal cannot be entered by walking: player=%s portal=%s blockedBy=%s impact=%s locked=%d overlapping=%d"),*Player->GetActorLocation().ToString(),*Portal->GetActorLocation().ToString(),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),Portal->IsLocked(),Portal->Trigger->IsOverlappingActor(Player)));return true;
                }
                Player->AddMovementInput(Direction,.3f);
            }
            return false;
        }
        if(Step==20)
        {
            for(TActorIterator<AHSPortal> It(W);It;++It)if(It->RoomRules&&It->RoomRules->RoomId==TEXT("RoomC"))Portal=*It;
            if(!Portal.IsValid()){Test->AddError(TEXT("Final portal missing"));return true;}
            Test->TestTrue(TEXT("Final item starts chase"),P->bFinalChaseStarted);
            Test->TestTrue(TEXT("A monster delayed entrance completes"),Room->Pursuer->bPursuitEnabled);
            bool Fell=false;for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It)Fell|=It->bCollapsed;
            Test->TestTrue(TEXT("A physical bookshelf collapsed"),Fell);
            Room->Pursuer->bCanDamagePlayer=true;
            for(int32 I=0;I<3;++I){Player->HitProtectionRemaining=0;Test->TestTrue(TEXT("Final-stage contact applies damage"),Player->ReceiveMonsterContact(Room->Pursuer));}
            Test->TestTrue(TEXT("Final-stage death begins blood-white recovery"),S->bDeathTransition);Step=22;return false;
        }
        if(Step==22)
        {
            Test->TestEqual(TEXT("Final-stage recovery restores three lives"),S->Lives,3);
            Test->TestEqual(TEXT("Final-stage recovery keeps the chapter"),P->StoryStep,6);
            Test->TestTrue(TEXT("Final-stage recovery keeps the final key"),P->HasStageClue(3,TEXT("RoomA"),TEXT("FinalMessage")));
            bool Fell=false;for(TActorIterator<AHSCollapsingObstacle> It(W);It;++It)Fell|=It->bCollapsed;
            Test->TestTrue(TEXT("Death preserves furniture collapse"),Fell);Test->TestTrue(TEXT("Death returns to visited A safety"),Room->IsSafe(Player));
            if(!Plan(W,Player,Portal->GetActorLocation()))return true;SawWhite=SawBlack=false;Step=21;return false;
        }
        if(Step==21)
        {
            if(P->bCompleted){Step=90;return false;}
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;
            if(Result>0)Player->AddMovementInput((Portal->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D(),.3f);
        }
        return false;
    }
};

class FTimeoutRecovery : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<UWorld> PreviousWorld;
public:
    explicit FTimeoutRecovery(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>40){Test->AddError(TEXT("Timeout recovery scenario stalled"));return true;}
        UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::Game||Context.WorldType==EWorldType::PIE)W=Context.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;auto* Room=Cast<AHSStoryDirector>(AHSRoomDirector::Find(W));
        if(!Room||!Player||W->GetTimeSeconds()<1.f)return false;
        auto* S=PC->GetSession();auto* P=Room->Progress();
        if(Step==0)
        {
            Test->TestTrue(TEXT("Fresh arrival in safety"),Room->IsSafe(Player));
            AHSPickup* Photo=nullptr;for(TActorIterator<AHSPickup> It(W);It;++It)if(It->ClueId==TEXT("Photo_A"))Photo=*It;
            if(!Photo){Test->AddError(TEXT("Missing photo for room rollback"));return true;}
            Player->SetActorLocation(Photo->GetActorLocation()+FVector(-200,0,70));Room->Tick(.01f);
            Test->TestTrue(TEXT("Test acquires current-room clue"),Photo->TryPickup(Player));Player->CancelAutoInspection();
            Test->TestTrue(TEXT("Acquired clue is present before expiration"),P->HasClue(TEXT("Photo_A")));
            PreviousWorld=W;Room->Tick(61.f);Step=1;return false;
        }
        if(Step==1)
        {
            if(PreviousWorld.Get()==W)return false;
            Test->TestEqual(TEXT("Timeout retains stage one"),P->Stage,1);Test->TestEqual(TEXT("Timeout retains current objective"),P->StoryStep,0);
            Test->TestTrue(TEXT("Timeout returns player to safety"),Room->IsSafe(Player));Test->TestEqual(TEXT("Timeout resets full budget"),P->GetRemaining(),60.f);
            Test->TestFalse(TEXT("Timeout stops timer inside safety"),P->bTimerRunning);Test->TestFalse(TEXT("Expired room clue is rolled back"),P->HasClue(TEXT("Photo_A")));
            Test->TestNull(TEXT("Expired room item is removed"),S->GetSelectedItem());Test->TestFalse(TEXT("Timeout restores player controls"),PC->IsGameplayLocked());
            Test->TestFalse(TEXT("Timeout does not trap player behind locked door"),Room->SafeDoor->bLocked);
            bool Restored=false;for(TActorIterator<AHSPickup> It(W);It;++It)Restored|=It->ClueId==TEXT("Photo_A");Test->TestTrue(TEXT("Expired clue is available for retry"),Restored);
            return true;
        }
        return false;
    }
};

class FFinalNavigation : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 RoomIndex=0;
    bool Moving=false;
    double Began=FPlatformTime::Seconds(),MovingAt=0;
    FVector MonsterStart;
    TWeakObjectPtr<AHSMonster> Monster;
public:
    explicit FFinalNavigation(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Game||C.WorldType==EWorldType::PIE)W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        if(!Player)return false;
        if(FPlatformTime::Seconds()-Began>45){Test->AddError(TEXT("Final chase navigation timed out"));return true;}
        if(W->GetTimeSeconds()<1||UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W))return false;
        auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
        if(!Moving)
        {
            const FName Rooms[]={TEXT("RoomA"),TEXT("RoomB"),TEXT("RoomC")};
            const FVector Targets[]={FVector(-900,500,192),FVector(-600,-3400,192),FVector(-200,-7900,190)};
            AHSStoryDirector* Director=nullptr;for(TActorIterator<AHSStoryDirector> It(W);It;++It)if(It->Rules->RoomId==Rooms[RoomIndex])Director=*It;
            if(!Director){Test->AddError(TEXT("Final chase director missing"));return true;}
            for(TActorIterator<AHSMonster> It(W);It;++It){It->bCanDamagePlayer=false;It->bPursuitEnabled=false;It->SetActorEnableCollision(false);}
            P->SetStage(3);P->StoryStep=6;P->bFinalChaseStarted=true;P->Clues.Add(TEXT("3:RoomA:FinalMessage"));
            Player->SetActorLocation(Targets[RoomIndex]);Player->GetCharacterMovement()->StopMovementImmediately();PC->SetGameplayLocked(false);
            Director->Tick(.01f);P->BeginCountdown();Monster=Director->Pursuer;Monster->bPursuitEnabled=true;Monster->bCinematicActor=false;
            Monster->SetActorEnableCollision(true);Monster->GetCharacterMovement()->SetMovementMode(MOVE_Walking);MonsterStart=Monster->GetActorLocation();
            auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,MonsterStart,Player->GetActorLocation(),Monster.Get());
            if(!Path||!Path->IsValid()||Path->IsPartial())
            {
                FNavLocation From,To;auto* Nav=UNavigationSystemV1::GetCurrent(W);
                const bool FoundFrom=Nav->ProjectPointToNavigation(MonsterStart,From,FVector(100,100,300));
                const bool FoundTo=Nav->ProjectPointToNavigation(Player->GetActorLocation(),To,FVector(100,100,300));
                Test->AddInfo(FString::Printf(TEXT("Navigation diagnosis: from=%s projected=%d %s to=%s projected=%d %s partial=%d"),*MonsterStart.ToString(),FoundFrom,*From.Location.ToString(),*Player->GetActorLocation().ToString(),FoundTo,*To.Location.ToString(),Path?Path->IsPartial():true));
                for(float Y:{-8400.f,-8300.f,-8200.f,-8100.f,-8000.f})
                {
                    FNavLocation Candidate;const bool Found=Nav->ProjectPointToNavigation(FVector(-200,Y,192),Candidate,FVector(50,50,300));
                    auto* CandidatePath=Found?UNavigationSystemV1::FindPathToLocationSynchronously(W,Candidate.Location,Player->GetActorLocation(),Monster.Get()):nullptr;
                    Test->AddInfo(FString::Printf(TEXT("C exit spawn probe Y=%.0f found=%d floor=%s fullPath=%d"),Y,Found,*Candidate.Location.ToString(),CandidatePath&&CandidatePath->IsValid()&&!CandidatePath->IsPartial()));
                }
            }
            Test->TestTrue(*FString::Printf(TEXT("%s ghost has complete navigation route"),*Rooms[RoomIndex].ToString()),Path&&Path->IsValid()&&!Path->IsPartial());
            MovingAt=FPlatformTime::Seconds();Moving=true;return false;
        }
        if(FPlatformTime::Seconds()-MovingAt<3)return false;
        Test->TestTrue(TEXT("Final-stage ghost actually moves toward player"),Monster.IsValid()&&FVector::Dist2D(MonsterStart,Monster->GetActorLocation())>100);
        Test->AddInfo(FString::Printf(TEXT("Final room %d monster movement: %s -> %s"),RoomIndex,*MonsterStart.ToString(),*Monster->GetActorLocation().ToString()));
        ++RoomIndex;Moving=false;return RoomIndex==3;
    }
};

// Isolate C's table lure with live AI and real movement; contact damage is tested separately.
class FFinalTableLure : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;FRoomWalker Walker;int32 Step=0;
    double Began=FPlatformTime::Seconds(),WaitAt=0;
    TWeakObjectPtr<AHSStoryDirector> Director;TWeakObjectPtr<AHSPortal> Exit;
    bool Plan(UWorld* W,AHSCharacter* Player,const FVector& Target)
    {
        if(!Walker.Plan(W,Player,Target,true,Test))return false;
        // Sprint uninterrupted along straight runs while retaining corners and floor-height changes.
        for(int32 I=Walker.Path.Num()-2;I>0;--I)
        {
            const FVector A=Walker.Path[I]-Walker.Path[I-1],B=Walker.Path[I+1]-Walker.Path[I];
            if(FVector::DotProduct(A.GetSafeNormal(),B.GetSafeNormal())>.999f)Walker.Path.RemoveAt(I);
        }
        return true;
    }
public:
    explicit FFinalTableLure(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Began>75){Test->AddError(FString::Printf(TEXT("C table lure timed out at step %d"),Step));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Game||C.WorldType==EWorldType::PIE)W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;
        if(!PC)return false;auto* S=PC->GetSession();
        if(Step==5&&S->bTitleScreen){Test->TestTrue(TEXT("C table lure reaches ending title"),true);return true;}
        auto* Player=Cast<AHSCharacter>(PC->GetPawn());if(!Player||S->bWhiteTransition)return false;
        auto* P=W->GetGameInstance()->GetSubsystem<UHSProgression>();
        if(Step==0)
        {
            if(W->GetTimeSeconds()<1||UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W))return false;
            for(TActorIterator<AHSStoryDirector> It(W);It;++It)if(It->Rules->RoomId==TEXT("RoomC"))Director=*It;
            if(!Director.IsValid()){Test->AddError(TEXT("Combined C director missing"));return true;}
            for(TActorIterator<AHSPortal> It(W);It;++It)if(It->RoomRules==Director->Rules)Exit=*It;
            P->SetStage(3);P->StoryStep=6;P->bFinalChaseStarted=true;P->Clues.Add(TEXT("3:RoomA:FinalMessage"));
            Player->SetActorLocation(FVector(-200,-5547,292));Player->GetCharacterMovement()->StopMovementImmediately();Director->Tick(.01f);
            for(TActorIterator<AHSMonster> It(W);It;++It)It->bCanDamagePlayer=false;
            if(!Plan(W,Player,FVector(-900,-7100,194)))return true;Step=1;return false;
        }
        if(Step==1)
        {
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;if(!Result)return false;
            Test->TestTrue(TEXT("Approaching C's table activates exit blocker"),Director->Pursuer->bPursuitEnabled);
            WaitAt=FPlatformTime::Seconds();Step=2;return false;
        }
        if(Step==2)
        {
            const FVector Ghost=Director->Pursuer->GetActorLocation();
            if(Ghost.Y<-7650||Ghost.X>-750)
            {
                if(FPlatformTime::Seconds()-WaitAt>12){Test->AddError(FString::Printf(TEXT("Exit ghost cannot be lured around C table: %s"),*Ghost.ToString()));return true;}
                return false;
            }
            Test->AddInfo(FString::Printf(TEXT("Player lured exit ghost to west of table: %s"),*Ghost.ToString()));
            if(!Plan(W,Player,FVector(-200,-6500,194)))return true;Step=3;return false;
        }
        if(Step==3||Step==4)
        {
            const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;if(!Result)return false;
            if(Step==3){if(!Plan(W,Player,FVector(-100,-7900,194)))return true;Step=4;return false;}
            Test->AddInfo(TEXT("Player crossed north and east sides of C's table with live pursuit"));
            if(!Exit.IsValid()){Test->AddError(TEXT("Final exit missing"));return true;}
            if(!Plan(W,Player,Exit->GetActorLocation()))return true;Step=5;return false;
        }
        if(Step==5){const int32 Result=Walker.Tick(PC,Player,Test);if(Result<0)return true;}
        return false;
    }
};

class FPaintingInteraction : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int32 Step=0;double Until=FPlatformTime::Seconds()+1;
    TWeakObjectPtr<AHSSwapPainting> First,Second;FVector FirstLocation,SecondLocation;
    void Aim(AHSPlayerController* PC,AHSCharacter* Player,AHSSwapPainting* Painting)
    {
        const FVector Center=Painting->Mesh->Bounds.Origin;
        Player->GetCharacterMovement()->StopMovementImmediately();Player->SetActorLocation(FVector(Center.X,Center.Y+160,194));
        PC->SetControlRotation((Center-Player->Camera->GetComponentLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);Player->RefreshInteractionFocus();
    }
public:
    explicit FPaintingInteraction(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Until)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Game||C.WorldType==EWorldType::PIE)W=C.World();
        auto* PC=W?Cast<AHSPlayerController>(UGameplayStatics::GetPlayerController(W,0)):nullptr;auto* Player=PC?Cast<AHSCharacter>(PC->GetPawn()):nullptr;
        if(!Player){Test->AddError(TEXT("Painting test requires B map"));return true;}
        if(Step==0)
        {
            for(TActorIterator<AHSSwapPainting> It(W);It;++It)if(FVector::Dist(It->GetActorLocation(),It->Mesh->Bounds.Origin)>100){First=*It;break;}
            if(!First.IsValid()){Test->AddError(TEXT("Offset authored painting missing"));return true;}
            float Best=MAX_flt;for(TActorIterator<AHSSwapPainting> It(W);It;++It)if(*It!=First.Get())
            {const float D=FVector::Dist(It->Mesh->Bounds.Origin,First->Mesh->Bounds.Origin);if(D<Best){Best=D;Second=*It;}}
            if(!Second.IsValid()){Test->AddError(TEXT("Second painting missing"));return true;}
            FirstLocation=First->Mesh->Bounds.Origin;SecondLocation=Second->Mesh->Bounds.Origin;Aim(PC,Player,First.Get());
            Until=FPlatformTime::Seconds()+.2;Step=1;return false;
        }
        if(Step==1)
        {
            Player->RefreshInteractionFocus();Test->AddInfo(FString::Printf(TEXT("Painting focus distance: actor %.1f, visible mesh %.1f"),FVector::Dist(Player->GetActorLocation(),First->GetActorLocation()),FVector::Dist(Player->GetActorLocation(),First->Mesh->Bounds.Origin)));
            if(Player->FocusedInteraction.Get()!=First.Get())
            {
                FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
                for(bool Complex:{false,true})
                {
                    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(PaintingFocusDiagnosis),Complex,Player);
                    W->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*360,ECC_Visibility,Q);
                    Test->AddInfo(FString::Printf(TEXT("Painting view diagnosis: complex=%d eye=%s target=%s hit=%s component=%s position=%s focused=%s"),Complex,*Eye.ToString(),*First->Mesh->Bounds.Origin.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),*GetNameSafe(Player->FocusedInteraction.Get())));
                }
            }
            Test->TestEqual(TEXT("Visible nearby painting can be focused"),Player->FocusedInteraction.Get(),static_cast<AActor*>(First.Get()));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));Until=FPlatformTime::Seconds()+.1;Step=2;return false;
        }
        if(Step==2)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));Test->TestTrue(TEXT("First E selects painting without moving it"),Player->SelectedPainting.Get()==First.Get()&&!First->bSwapping&&First->Mesh->Bounds.Origin.Equals(FirstLocation));
            Aim(PC,Player,Second.Get());Until=FPlatformTime::Seconds()+.2;Step=3;return false;
        }
        if(Step==3){PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));Until=FPlatformTime::Seconds()+.1;Step=4;return false;}
        if(Step==4){PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));Until=FPlatformTime::Seconds()+1.1;Step=5;return false;}
        Test->TestTrue(TEXT("Two selected paintings exchange visible positions"),First->Mesh->Bounds.Origin.Equals(SecondLocation,1)&&Second->Mesh->Bounds.Origin.Equals(FirstLocation,1));
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSPhysicalStoryTest,"HorrorSystems.Jam.PhysicalWalkthrough",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSPhysicalStoryTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FPhysicalStory(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSTimeoutRecoveryTest,"HorrorSystems.Jam.TimeoutRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSTimeoutRecoveryTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTimeoutRecovery(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSFinalNavigationTest,"HorrorSystems.Jam.FinalChaseNavigation",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSFinalNavigationTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FFinalNavigation(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSFinalTableLureTest,"HorrorSystems.Jam.FinalTableLure",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSFinalTableLureTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FFinalTableLure(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSPaintingInteractionTest,"HorrorSystems.Jam.PaintingInteraction",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSPaintingInteractionTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FPaintingInteraction(this));return true;}
#endif
