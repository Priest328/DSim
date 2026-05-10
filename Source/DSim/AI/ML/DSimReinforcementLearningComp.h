#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSimReinforcementLearningComp.generated.h"

class ADSimCharacterAIController;
class ADSimCharacter;

/**
 * Набір доступних дискретних дій агента
 */
UENUM(BlueprintType)
enum class EBotAction : uint8
{
	None UMETA(DisplayName = "None"),
	TowardGoal UMETA(DisplayName = "Toward Goal"),
	TowardCover UMETA(DisplayName = "Toward Cover"),
	RandomMove UMETA(DisplayName = "Random Move"),
};

USTRUCT(BlueprintType)
struct FDSimBotActionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction BotAction = EBotAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float QValue = 0.0f;
};

/**
 * Legacy 1D дані (для міграції зі старого RLData.json)
 */
USTRUCT(BlueprintType)
struct FDSimRLSectionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SectionKey = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimBotActionData> Actions;

	FDSimBotActionData* FindAction(EBotAction Action)
	{
		for (auto& A : Actions)
		{
			if (A.BotAction == Action)
			{
				return &A;
			}
		}
		return nullptr;
	}
};

USTRUCT(BlueprintType)
struct FDSimRLData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimRLSectionData> AllSections;

	FDSimRLSectionData* FindSection(float InSectionKey)
	{
		for (auto& S : AllSections)
		{
			if (FMath::IsNearlyEqual(S.SectionKey, InSectionKey, KINDA_SMALL_NUMBER))
			{
				return &S;
			}
		}
		return nullptr;
	}
};

// ==============================
// TO-BE 2D state model
// ==============================
USTRUCT(BlueprintType)
struct FDSimRLStateKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SectionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 LaneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PackedKey = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SectionKey = 0.f;

	void RebuildPackedKey(int32 InNumLanes)
	{
		const int32 SafeLanes = FMath::Max(1, InNumLanes);
		PackedKey = SectionIndex * SafeLanes + LaneIndex;
	}
};

USTRUCT(BlueprintType)
struct FDSimRLStateData2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDSimRLStateKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimBotActionData> Actions;

	FDSimBotActionData* FindAction(EBotAction Action)
	{
		for (auto& A : Actions)
		{
			if (A.BotAction == Action)
			{
				return &A;
			}
		}
		return nullptr;
	}
};

USTRUCT(BlueprintType)
struct FDSimRLData2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumLanes = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LaneHalfWidth = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimRLStateData2D> AllStates;
};

USTRUCT(BlueprintType)
struct FEpisodeStep
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PackedKey = 0;

	UPROPERTY()
	int32 SectionIndex = 0;

	UPROPERTY()
	int32 LaneIndex = 0;

	UPROPERTY()
	float SectionKey = 0.f;

	UPROPERTY()
	EBotAction Action = EBotAction::None;

	UPROPERTY()
	float Reward = 0.f;
};

USTRUCT(BlueprintType)
struct FRLSectionStepDebug
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SectionIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 LaneIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PackedKey = 0;

	UPROPERTY(BlueprintReadOnly)
	float SectionKey = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float QTowardGoal = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float QTowardCover = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float QRandomMove = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float InitialQTowardGoal = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float InitialQTowardCover = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float InitialQRandomMove = 0.f;
};

USTRUCT(BlueprintType)
struct FRLDebugRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentSectionIndex = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentLaneIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	float CurrentSectionKey = 0.f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FRLSectionStepDebug> NextSections;

	UPROPERTY(BlueprintReadOnly)
	TArray<FRLSectionStepDebug> PreviousSections;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDebugDataUpdatedSignature, FRLDebugRuntimeData, DebugData);

// ==============================
// Strategy / Repository interfaces
// ==============================
class IRLStateDiscretizer
{
public:
	virtual ~IRLStateDiscretizer() = default;

	virtual FDSimRLStateKey Discretize(
		const FVector& Pos,
		const FVector& Start,
		const FVector& Goal,
		float PathLength,
		int32 NumSections,
		int32 NumLanes,
		float LaneHalfWidth) const = 0;
};

class IRLActionPolicy
{
public:
	virtual ~IRLActionPolicy() = default;
	virtual EBotAction SelectAction(const TArray<FDSimBotActionData>& Actions, float Epsilon) const = 0;
};

class IRLDataRepository
{
public:
	virtual ~IRLDataRepository() = default;
	virtual bool Save(const FDSimRLData2D& Data, const FString& Path) = 0;
	virtual bool Load(FDSimRLData2D& OutData, const FString& Path) = 0;
	virtual bool LoadLegacy1D(FDSimRLData& OutLegacy, const FString& Path) = 0;
};

// ==============================
// RL component
// ==============================
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DSIM_API UDSimReinforcementLearningComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimReinforcementLearningComp();

	void InitComponentData();
	EBotAction RequestAction();
	void ApplyReward(float Reward, bool bEpisodeEnd);

	UFUNCTION(BlueprintCallable)
	bool SaveRLDataToFile();

	UFUNCTION(BlueprintCallable)
	bool LoadRLDataFromFile();

	UFUNCTION(BlueprintCallable, Category = "RL|Debug")
	void SetCurrentMoveTarget(const FVector& NewTarget);

	UFUNCTION(BlueprintCallable, Category = "RL|Debug")
	void SetDrawDebug(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "RL|State")
	void SetGoalPosition(const FVector& NewGoalPosition);

	UFUNCTION(BlueprintPure, Category = "RL|Debug")
	const FRLDebugRuntimeData& GetDebugData() const { return DebugData; }

	UPROPERTY(BlueprintAssignable)
	FOnDebugDataUpdatedSignature OnDebugDataUpdated;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnEpisodeEnd();
	void StartNewEpisode();
	void DecayEpsilon();
	void LogRLDebug(const FString& Msg);

	void UpdateDebugNextSections(const FDSimRLStateKey& CurrentKey);
	void ExtractQValues(const TArray<FDSimBotActionData>& Actions, float& OutGoal, float& OutCover, float& OutRandom) const;

	FString GetRLDataSavePath() const;
	void RebuildStateIndex();

	FDSimRLStateData2D* FindOrAddState(const FDSimRLStateKey& Key);
	FDSimRLStateData2D* FindStateByPacked(int32 PackedKey);

private:
	UPROPERTY()
	TObjectPtr<ADSimCharacter> OwnerActor;

	UPROPERTY()
	TObjectPtr<ADSimCharacterAIController> AIController;

	UPROPERTY()
	FDSimRLData2D RLData2D;

	UPROPERTY()
	TArray<FEpisodeStep> CurrentEpisode;

	TArray<TArray<FEpisodeStep>> ReplayBuffer;

	UPROPERTY(EditAnywhere, Category = "RL")
	int32 ReplayBufferSize = 100;

	UPROPERTY(EditAnywhere, Category = "RL")
	float Gamma = 0.95f;

	UPROPERTY(EditAnywhere, Category = "RL")
	float Alpha = 0.3f;

	UPROPERTY(EditAnywhere, Category = "RL")
	float Epsilon = 0.2f;

	UPROPERTY(EditAnywhere, Category = "RL")
	float EpsilonMin = 0.05f;

	UPROPERTY(EditAnywhere, Category = "RL")
	float EpsilonDecay = 0.99f;

	UPROPERTY(EditAnywhere, Category = "RL|State")
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, Category = "RL|State")
	bool bUse2DState = true;

	UPROPERTY(EditAnywhere, Category = "RL|State", meta = (EditCondition = "bUse2DState"))
	int32 NumLanes = 5;

	UPROPERTY(EditAnywhere, Category = "RL|State", meta = (EditCondition = "bUse2DState"))
	float LaneHalfWidth = 1500.f;

	UPROPERTY(EditAnywhere, Category = "RL")
	float CriticalDroneDistance = 900.f;

	UPROPERTY(EditAnywhere, Category = "RL|IO")
	FString SaveFileName1D = TEXT("RLData_1D.json");

	UPROPERTY(EditAnywhere, Category = "RL|IO")
	FString SaveFileName2D = TEXT("RLData_2D.json");

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKey;

	FVector StartPosition = FVector::ZeroVector;
	FVector GoalPosition = FVector::ZeroVector;
	float PathLength = 1.f;

	bool bHasLastEnteredSection = false;
	FRLSectionStepDebug LastEnteredSection;

public:
	UPROPERTY(EditAnywhere, Category = "RL|Debug")
	bool bDrawDebug = false;

	UPROPERTY(VisibleAnywhere, Category = "RL|Debug")
	FVector CurrentMoveTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "RL|Debug", meta = (AllowPrivateAccess = "true"))
	FRLDebugRuntimeData DebugData;

	UPROPERTY()
	TArray<FRLSectionStepDebug> LastChangedSections;

private:
	TUniquePtr<IRLStateDiscretizer> StateDiscretizer;
	TUniquePtr<IRLActionPolicy> ActionPolicy;
	TUniquePtr<IRLDataRepository> DataRepository;
};
