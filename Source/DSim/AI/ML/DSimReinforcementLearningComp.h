#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/ML/DSimReinforcementLearningConfig.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimReinforcementLearningComp.generated.h"

class ADSimCharacterAIController;
class ADSimCharacter;

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
 * Legacy 1D data, used for migration from old RLData.json files.
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
		for (FDSimBotActionData& A : Actions)
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
		for (FDSimRLSectionData& S : AllSections)
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
// 2D state model
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
		for (FDSimBotActionData& A : Actions)
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
class DSIM_API UDSimReinforcementLearningComp : public UDSimBotTrainingAlgorithmComponent
{
	GENERATED_BODY()

public:
	UDSimReinforcementLearningComp();

	void InitComponentData();

	// Legacy direct API. BT and SimulationManager should use RequestTrainingAction()/AddTrainingReward()/EndEpisode().
	EBotAction RequestAction();
	void ApplyReward(float Reward, bool bEpisodeEnd);

	virtual void InitializeAlgorithm(
		const FDSimAlgorithmRuntimeContext& InContext,
		UDSimBotTrainingAlgorithmConfig* InConfig
	) override;

	virtual void StartEpisode(int32 EpisodeId) override;
	virtual void EndEpisode(EDSimEpisodeFinishReason FinishReason) override;
	virtual EBotAction RequestTrainingAction() override;
	virtual void AddTrainingReward(float Reward) override;
	virtual void SetGoalPosition(const FVector& NewGoalPosition) override;
	virtual bool LoadTrainingData(const FString& FileName) override;
	virtual bool SaveTrainingData(const FString& FileName) override;
	virtual FString GetAlgorithmName() const override;

	UFUNCTION(BlueprintCallable)
	bool SaveRLDataToFile();

	UFUNCTION(BlueprintCallable)
	bool LoadRLDataFromFile();

	UFUNCTION(BlueprintCallable, Category = "RL|Debug")
	void SetCurrentMoveTarget(const FVector& NewTarget);

	UFUNCTION(BlueprintCallable, Category = "RL|Debug")
	void SetDrawDebug(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "RL|Debug")
	const FRLDebugRuntimeData& GetDebugData() const { return DebugData; }

	UPROPERTY(BlueprintAssignable)
	FOnDebugDataUpdatedSignature OnDebugDataUpdated;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ResolveOwnerReferences();
	void ApplyRLConfig(const UDSimReinforcementLearningConfig* Config);
	const UDSimReinforcementLearningConfig* GetRLConfig() const;

	void OnEpisodeEnd();
	void StartNewEpisode();
	void DecayEpsilon();
	void LogRLDebug(const FString& Msg);

	float GetTerminalRewardForFinishReason(EDSimEpisodeFinishReason FinishReason) const;
	FString ResolveTrainingDataPath(const FString& FileName) const;

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

	// Runtime copies from UDSimReinforcementLearningConfig.
	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	int32 ReplayBufferSize = 100;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float Gamma = 0.95f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float Alpha = 0.3f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float CurrentEpsilon = 0.2f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float EpsilonMin = 0.05f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float EpsilonDecay = 0.99f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	int32 NumSections = 100;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	bool bUse2DState = true;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	int32 NumLanes = 5;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float LaneHalfWidth = 1500.f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	float CriticalDroneDistance = 900.f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Reward")
	float GoalReachedTerminalReward = 1.0f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Reward")
	float BotKilledTerminalReward = -1.0f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Reward")
	float DroneCrashedTerminalReward = 0.4f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Reward")
	float TimeoutTerminalReward = -0.2f;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	bool bCurrentEpisodeFinalized = false;

	UPROPERTY(VisibleAnywhere, Category = "RL|Runtime")
	bool bUseExperimentOutputFiles = false;

	// Legacy fallback file names for manual tests outside SimulationManager.
	UPROPERTY(VisibleAnywhere, Category = "RL|Legacy IO")
	FString SaveFileName1D = TEXT("RLData_1D.json");

	UPROPERTY(VisibleAnywhere, Category = "RL|Legacy IO")
	FString SaveFileName2D = TEXT("RLData_2D.json");

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKey;

	FVector StartPosition = FVector::ZeroVector;
	FVector GoalPosition = FVector::ZeroVector;
	float PathLength = 1.f;

	bool bHasLastEnteredSection = false;
	FRLSectionStepDebug LastEnteredSection;

public:
	UPROPERTY(VisibleAnywhere, Category = "RL|Debug")
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
