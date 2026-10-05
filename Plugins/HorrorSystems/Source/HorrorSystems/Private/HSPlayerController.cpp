#include "HSPlayerController.h"
#include "HSCharacter.h"
#include "HSWorldState.h"
#include "HSItemData.h"
#include "HSOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Camera/PlayerCameraManager.h"

AHSPlayerController::AHSPlayerController() { PrimaryActorTick.bTickEvenWhenPaused=true; }
void AHSPlayerController::BeginPlay()
{
    Super::BeginPlay();
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
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this,I]{ SelectSlot(I); });
        InputComponent->KeyBindings.Add(Binding);
    }
}
UHSWorldState* AHSPlayerController::GetSession() const { return GetGameInstance()?GetGameInstance()->GetSubsystem<UHSWorldState>():nullptr; }
void AHSPlayerController::SelectSlot(int32 Index) { if(!bInspecting) if(auto* S=GetSession()) S->SelectSlot(Index); }
void AHSPlayerController::Notify(const FText& Text,float Seconds) { Notification=Text; NotificationUntil=FPlatformTime::Seconds()+Seconds; }
FText AHSPlayerController::GetNotification() const { return FPlatformTime::Seconds()<NotificationUntil?Notification:FText::GetEmpty(); }
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
    if(auto* C=Cast<AHSCharacter>(GetPawn())) C->CancelAutoInspection();
    InspectionItem=Item; bInspecting=true;
    bOwnsPause=!UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this,true);
    if(Item->InspectSound) UGameplayStatics::PlaySound2D(this,Item->InspectSound,.5f,1.f,0.f,nullptr,nullptr,true);
    ApplyInputMode();
}
void AHSPlayerController::CloseInspection()
{
    if(!bInspecting) return;
    bInspecting=false; InspectionItem=nullptr;
    if(bOwnsPause) UGameplayStatics::SetGamePaused(this,false);
    bOwnsPause=false; ApplyInputMode();
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
