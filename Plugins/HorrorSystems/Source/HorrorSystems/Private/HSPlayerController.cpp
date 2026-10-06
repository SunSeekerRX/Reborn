#include "HSPlayerController.h"
#include "HSCharacter.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture2D.h"
#include "HSProgression.h"
#include "HSSettings.h"
#include "Components/AudioComponent.h"

AHSPlayerController::AHSPlayerController() { PrimaryActorTick.bTickEvenWhenPaused=true; }
void AHSPlayerController::BeginPlay()
{
    Super::BeginPlay();
    GetSession()->bTitleScreen=UGameplayStatics::GetCurrentLevelName(this,true)==TEXT("RebornTitle");
    GetGameInstance()->GetSubsystem<UHSProgression>()->MapVisits.FindOrAdd(FName(*UGameplayStatics::GetCurrentLevelName(this,true)))++;
    if(GetSession()->bTitleScreen) {bGameplayLocked=true;TitleTexture=LoadObject<UTexture2D>(nullptr,TEXT("/HorrorSystems/UI/T_MobiusPixel.T_MobiusPixel"));}
    if(GetSession()->bTitleScreen) if(auto* Sound=GetDefault<UHSSettings>()->ChaseMusic.LoadSynchronous()) TitleMusic=UGameplayStatics::SpawnSound2D(this,Sound,.24f);
    if(GetSession()->bTitleScreen) if(auto* Sound=GetDefault<UHSSettings>()->TitleRevealSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this,Sound,.4f);
    if(PlayerCameraManager) { PlayerCameraManager->ViewPitchMin=-80.f; PlayerCameraManager->ViewPitchMax=80.f; }
    FRotator InitialView=GetControlRotation(); InitialView.Pitch=-8.f; SetControlRotation(InitialView);
    if(IsLocalController() && GetWorld()->GetGameViewport())
    {
        SAssignNew(Overlay,SHSOverlay).Controller(this);
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(Overlay.ToSharedRef(),20);
        ApplyInputMode();
    }
}
void AHSPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if(TitleMusic) TitleMusic->Stop();
    if(Overlay.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset(); Super::EndPlay(Reason);
}
void AHSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AHSPlayerController::ToggleHotbarMouse).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AHSPlayerController::InspectSelected).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AHSPlayerController::Escape).bExecuteWhenPaused=true;
    const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,EKeys::Zero};
    for(int32 I=0;I<10;++I)
    {
        FInputKeyBinding Binding(FInputChord(Keys[I]),IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this,I]{ ActivateNumberSlot(I); });
        InputComponent->KeyBindings.Add(Binding);
    }
}
UHSWorldState* AHSPlayerController::GetSession() const { return GetGameInstance()?GetGameInstance()->GetSubsystem<UHSWorldState>():nullptr; }
bool AHSPlayerController::IsMouseMode() const {return bHotbarMouse || bInspecting || (GetSession() && GetSession()->bTitleScreen);}
void AHSPlayerController::SelectSlot(int32 Index) { LastNumberSlot=INDEX_NONE; if(!bInspecting) if(auto* S=GetSession()) S->SelectSlot(Index); }
void AHSPlayerController::ActivateNumberSlot(int32 Index)
{
    auto* S=GetSession();
    if(!S || bInspecting || bGameplayLocked || !S->Slots.IsValidIndex(Index)) { LastNumberSlot=INDEX_NONE; return; }
    const double Now=FPlatformTime::Seconds();
    const bool bDouble=LastNumberSlot==Index && Now-LastNumberTime<=.35;
    SelectSlot(Index);
    LastNumberSlot=Index; LastNumberTime=Now;
    if(bDouble && S->GetSelectedItem()) {LastNumberSlot=INDEX_NONE; InspectItem(S->GetSelectedItem());}
}
void AHSPlayerController::Notify(const FText& Text,float Seconds) { Notification=Text; NotificationUntil=FPlatformTime::Seconds()+Seconds; }
FText AHSPlayerController::GetNotification() const { return FPlatformTime::Seconds()<NotificationUntil?Notification:FText::GetEmpty(); }
void AHSPlayerController::Speak(const FText& Text,float Seconds) {Subtitle=Text;SubtitleUntil=FPlatformTime::Seconds()+FMath::Max(.1f,Seconds);}
FText AHSPlayerController::GetSubtitle() const {return FPlatformTime::Seconds()<SubtitleUntil?Subtitle:FText::GetEmpty();}
void AHSPlayerController::ToggleHotbarMouse() { if(bGameplayLocked || bInspecting) return; bHotbarMouse=!bHotbarMouse; ApplyInputMode(); }
void AHSPlayerController::InspectSelected()
{
    if(bInspecting) { CloseInspection(); return; }
    if(auto* S=GetSession(); S && S->GetSelectedItem()) InspectItem(S->GetSelectedItem());
    else Notify(FText::FromString(TEXT("当前格子为空")));
}
void AHSPlayerController::InspectItem(UHSItemData* Item)
{
    if(!Item || bInspecting || bGameplayLocked) return;
    if(auto* C=Cast<AHSCharacter>(GetPawn()))
    {
        C->CancelAutoInspection();
        bInspectionOwnerNoSee=C->GetMesh()->bOwnerNoSee;
        C->GetMesh()->SetOwnerNoSee(true);
    }
    InspectionItem=Item; bInspecting=true;
    LastNumberSlot=INDEX_NONE;
    bOwnsPause=!UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this,true);
    if(Item->InspectSound) UGameplayStatics::PlaySound2D(this,Item->InspectSound,.5f,1.f,0.f,nullptr,nullptr,true);
    ApplyInputMode();
}
void AHSPlayerController::CloseInspection()
{
    if(!bInspecting) return;
    auto* ClosedItem=InspectionItem.Get();
    bInspecting=false; InspectionItem=nullptr;
    if(auto* C=Cast<AHSCharacter>(GetPawn())) C->GetMesh()->SetOwnerNoSee(bInspectionOwnerNoSee);
    if(bOwnsPause) UGameplayStatics::SetGamePaused(this,false);
    bOwnsPause=false; ApplyInputMode(); OnInspectionClosed.Broadcast(ClosedItem);
}
void AHSPlayerController::Escape() { if(bInspecting) CloseInspection(); else if(bHotbarMouse) { bHotbarMouse=false; ApplyInputMode(); } }
void AHSPlayerController::SetGameplayLocked(bool Locked) { bGameplayLocked=Locked; if(Locked) bHotbarMouse=false; ApplyInputMode(); }
void AHSPlayerController::ApplyInputMode()
{
    bShowMouseCursor=IsMouseMode();
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    SetIgnoreMoveInput(IsMouseMode() || bGameplayLocked); SetIgnoreLookInput(IsMouseMode() || bGameplayLocked);
    if(auto* C=Cast<AHSCharacter>(GetPawn())) C->ClearMovementModifiers();
    if(IsMouseMode())
    {
        FInputModeGameAndUI Mode; Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); Mode.SetHideCursorDuringCapture(false);
        if(Overlay.IsValid()) Mode.SetWidgetToFocus(Overlay);
        SetInputMode(Mode);
        if(Overlay.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Overlay,EFocusCause::SetDirectly);
    }
    else SetInputMode(FInputModeGameOnly());
}
