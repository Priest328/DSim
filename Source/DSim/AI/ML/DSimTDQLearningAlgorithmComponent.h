#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimTDQLearningAlgorithmComponent.generated.h"

class ADSimCharacter;
class ADSimCharacterAIController;
class UDSimTDQLearningAlgorithmConfig;

USTRUCT(BlueprintType)
struct FDSimTDQActionValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction BotAction = EBotAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float QValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FDSimTDQStateKey
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
struct FDSimTDQStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDSimTDQStateKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimTDQActionValue> Actions;

	FDSimTDQActionValue* FindAction(EBotAction Action)
	{
		for (FDSimTDQActionValue& A : Actions)
		{
			if (A.BotAction == Action)
			{
				return &A;
			}
		}
		return nullptr;
	}

	const FDSimTDQActionValue* FindAction(EBotAction Action) const
	{
		for (const FDSimTDQActionValue& A : Actions)
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
struct FDSimTDQTableData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumLanes = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LaneHalfWidth = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimTDQStateData> AllStates;
};

/**
 * Tabular TD Q-Learning component.
 *
 * Difference from episodic / Monte Carlo-like baseline:
 * this component updates Q(s,a) incrementally after each observed transition,
 * not only after the full episode ends.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DSIM_API UDSimTDQLearningAlgorithmComponent : public UDSimBotTrainingAlgorithmComponent
{
	GENERATED_BODY()

public:
	UDSimTDQLearningAlgorithmComponent();

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
	void InitializeEmptyTable();
	void RebuildStateIndex();

	FDSimTDQStateKey DiscretizeCurrentState() const;

	FDSimTDQStateData* FindStateByPackedKey(int32 PackedKey);
	FDSimTDQStateData* FindOrAddState(const FDSimTDQStateKey& Key);

	FDSimTDQActionValue* FindActionValue(FDSimTDQStateData& State, EBotAction Action);

	EBotAction SelectActionEpsilonGreedy(const FDSimTDQStateData& State) const;
	float GetMaxQValue(const FDSimTDQStateData& State) const;

	void ApplyTDUpdate(
		int32 StatePackedKey,
		EBotAction Action,
		float Reward,
		const FDSimTDQStateData* NextState,
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
	FDSimTDQTableData QTable;

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKey;

	UPROPERTY()
	TObjectPtr<UDSimTDQLearningAlgorithmConfig> TDConfig;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float Alpha = 0.3f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float Gamma = 0.95f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float Epsilon = 0.2f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float EpsilonMin = 0.05f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float EpsilonDecay = 0.99f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	int32 NumSections = 100;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	int32 NumLanes = 1;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float LaneHalfWidth = 1500.0f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	bool bUse2DState = true;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	float InitialQValue = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "TD Q-Learning|Runtime")
	bool bCurrentEpisodeFinalized = false;

	FVector StartPosition = FVector::ZeroVector;
	FVector GoalPosition = FVector::ZeroVector;
	float PathLength = 1.0f;

	bool bHasLastTransition = false;
	int32 LastStatePackedKey = INDEX_NONE;
	EBotAction LastAction = EBotAction::None;

	// Accumulates reward between the previous action and the next state observation.
	float PendingReward = 0.0f;

	FString DefaultSaveFileName = TEXT("TD_Q_Learning_Data.json");
};