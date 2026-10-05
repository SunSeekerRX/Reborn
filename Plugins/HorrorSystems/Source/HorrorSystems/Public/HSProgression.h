#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataAsset.h"
#include "HSItemData.h"
#include "HSProgression.generated.h"

USTRUCT(BlueprintType)
struct FHSRoomRoute
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Stage=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName TargetRoom;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UWorld> Destination;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FName> RequiredClues;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bAdvanceStage=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bFinishAtFinalStage=false;
};

UCLASS(BlueprintType)
class HORRORSYSTEMS_API UHSRoomRules : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName RoomId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText RoomName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float Duration=60.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FHSRoomRoute> Routes;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TMap<FName,FText> ClueLabels;
    const FHSRoomRoute* RouteFor(int32 Stage) const;
};

UCLASS()
class HORRORSYSTEMS_API UHSProgression : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Stage=1;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName ActiveRoom;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bTimerRunning=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bCinematic=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bCompleted=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TMap<FName,float> Remaining;
    UPROPERTY() TMap<FName,float> Budgets;
    UPROPERTY() TSet<FString> Clues;
    UPROPERTY() TSet<FName> SeenWindows;
    UFUNCTION(BlueprintCallable,Category="Progression") void EnterRoom(FName Room,float Duration);
    UFUNCTION(BlueprintCallable,Category="Progression") void BeginCountdown();
    // Returns true exactly when the active room's time expires.
    bool AdvanceClock(float Dt);
    UFUNCTION(BlueprintPure,Category="Progression") float GetRemaining() const;
    UFUNCTION(BlueprintCallable,Category="Progression") void CollectClue(FName Clue);
    UFUNCTION(BlueprintPure,Category="Progression") bool HasClue(FName Clue) const;
    bool CanExit(const FHSRoomRoute& Route) const;
    void CommitExit(const FHSRoomRoute& Route);
    UFUNCTION(BlueprintCallable,Category="Progression") void RollbackRoom();
    UFUNCTION(BlueprintCallable,Category="Progression") bool SetStage(int32 NewStage);
    UFUNCTION(BlueprintCallable,Category="Progression") void ResetProgression();
private:
    FString ClueKey(FName Clue) const;
    UPROPERTY() TArray<FHSItemSlot> EntrySlots;
    UPROPERTY() TSet<FString> EntryPickups;
    UPROPERTY() TSet<FString> EntryClues;
    int32 EntrySelection=0;
};
