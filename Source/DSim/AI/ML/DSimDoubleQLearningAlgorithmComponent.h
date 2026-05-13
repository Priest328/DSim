#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimDoubleQLearningAlgorithmComponent.generated.h"

class ADSimCharacter;
class ADSimCharacterAIController;
class UDSimDoubleQLearningAlgorithmConfig;

USTRUCT(BlueprintType)
struct FDSimDoubleQActionValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction BotAction = EBotAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float QValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FDSimDoubleQStateKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SectionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 LaneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PackedKey = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SectionKey = 0.0f;

	void RebuildPackedKey(int32 InNumLanes)
	{
		const int32 SafeLanes = FMath::Max(1, InNumLanes);
		PackedKey = SectionIndex * SafeLanes + LaneIndex;
	}
};

USTRUCT(BlueprintType)
struct FDSimDoubleQStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDSimDoubleQStateKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimDoubleQActionValue> Actions;

	FDSimDoubleQActionValue* FindAction(EBotAction Action)
	{
		for (FDSimDoubleQActionValue& A : Actions)
		{
			if (A.BotAction == Action)
			{
				return &A;
			}
		}

		return nullptr;
	}

	const FDSimDoubleQActionValue* FindAction(EBotAction Action) const
	{
		for (const FDSimDoubleQActionValue& A : Actions)
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
struct FDSimDoubleQTableData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumLanes = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LaneHalfWidth = 1500.0f;

	// First Q-table.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimDoubleQStateData> QTableA;

	// Second Q-table.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimDoubleQStateData> QTableB;
};

/**
 * Tabular Double Q-Learning component.
 *
 * The action policy uses QTableA + QTableB.
 * The update randomly updates only one table to reduce max-action overestimation.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DSIM_API UDSimDoubleQLearningAlgorithmComponent : public UDSimBotTrainingAlgorithmComponent
{
	GENERATED_BODY()

public:
	UDSimDoubleQLearningAlgorithmComponent();

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

private:
	void ResolveOwnerReferences();
	void InitializeEmptyTables();
	void RebuildStateIndexes();

	FDSimDoubleQStateKey DiscretizeCurrentState() const;

	FDSimDoubleQStateData* FindStateByPackedKey(
		TArray<FDSimDoubleQStateData>& Table,
		TMap<int32, int32>& IndexMap,
		int32 PackedKey
	);

	const FDSimDoubleQStateData* FindStateByPackedKey(
		const TArray<FDSimDoubleQStateData>& Table,
		const TMap<int32, int32>& IndexMap,
		int32 PackedKey
	) const;

	FDSimDoubleQStateData* FindOrAddState(
		TArray<FDSimDoubleQStateData>& Table,
		TMap<int32, int32>& IndexMap,
		const FDSimDoubleQStateKey& Key
	);

	void EnsureStateInBothTables(const FDSimDoubleQStateKey& Key);

	FDSimDoubleQActionValue* FindActionValue(FDSimDoubleQStateData& State, EBotAction Action);
	const FDSimDoubleQActionValue* FindActionValue(const FDSimDoubleQStateData& State, EBotAction Action) const;

	EBotAction SelectActionEpsilonGreedy(const FDSimDoubleQStateData& StateA, const FDSimDoubleQStateData& StateB) const;

	EBotAction GetBestActionFromTable(const FDSimDoubleQStateData& State) const;
	float GetQValueForAction(const FDSimDoubleQStateData& State, EBotAction Action) const;
	float GetCombinedQValue(const FDSimDoubleQStateData& StateA, const FDSimDoubleQStateData& StateB, EBotAction Action) const;

	void ApplyDoubleQUpdate(
		int32 StatePackedKey,
		EBotAction Action,
		float Reward,
		int32 NextStatePackedKey,
		bool bTerminal
	);

	void FlushPendingTransitionAsTerminal(float TerminalReward);
	void DecayEpsilon();

	float GetTerminalRewardForFinishReason(EDSimEpisodeFinishReason FinishReason) const;

	FString ResolveTrainingDataPath(const FString& FileName) const;

private:
	UPROPERTY()
	TObjectPtr<ADSimCharacterAIController> AIController;

	UPROPERTY()
	TObjectPtr<ADSimCharacter> OwnerActor;

	UPROPERTY()
	FDSimDoubleQTableData QData;

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKeyA;

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKeyB;

	UPROPERTY()
	TObjectPtr<UDSimDoubleQLearningAlgorithmConfig> DoubleQConfig;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float Alpha = 0.3f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float Gamma = 0.95f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float Epsilon = 0.2f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float EpsilonMin = 0.05f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float EpsilonDecay = 0.99f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float UpdateFirstTableProbability = 0.5f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	int32 NumSections = 100;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	int32 NumLanes = 1;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float LaneHalfWidth = 1500.0f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	bool bUse2DState = true;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	float InitialQValue = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Double Q-Learning|Runtime")
	bool bCurrentEpisodeFinalized = false;

	FVector StartPosition = FVector::ZeroVector;
	FVector GoalPosition = FVector::ZeroVector;
	float PathLength = 1.0f;

	bool bHasLastTransition = false;
	int32 LastStatePackedKey = INDEX_NONE;
	EBotAction LastAction = EBotAction::None;

	// Accumulates reward between the previous action and the next state observation.
	float PendingReward = 0.0f;

	FString DefaultSaveFileName = TEXT("Double_Q_Learning_Data.json");
};
