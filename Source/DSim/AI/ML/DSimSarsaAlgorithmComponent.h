#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimSarsaAlgorithmComponent.generated.h"

class ADSimCharacter;
class ADSimCharacterAIController;
class UDSimSarsaAlgorithmConfig;

USTRUCT(BlueprintType)
struct FDSimSarsaActionValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction BotAction = EBotAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float QValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FDSimSarsaStateKey
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
struct FDSimSarsaStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDSimSarsaStateKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimSarsaActionValue> Actions;

	FDSimSarsaActionValue* FindAction(EBotAction Action)
	{
		for (FDSimSarsaActionValue& A : Actions)
		{
			if (A.BotAction == Action)
			{
				return &A;
			}
		}

		return nullptr;
	}

	const FDSimSarsaActionValue* FindAction(EBotAction Action) const
	{
		for (const FDSimSarsaActionValue& A : Actions)
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
struct FDSimSarsaTableData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumLanes = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LaneHalfWidth = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimSarsaStateData> AllStates;
};

/**
 * Tabular SARSA component.
 *
 * SARSA updates Q-values using the next action that the policy actually selected.
 * This can produce a more cautious policy than off-policy Q-Learning when exploration is dangerous.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DSIM_API UDSimSarsaAlgorithmComponent : public UDSimBotTrainingAlgorithmComponent
{
	GENERATED_BODY()

public:
	UDSimSarsaAlgorithmComponent();

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

	FDSimSarsaStateKey DiscretizeCurrentState() const;

	FDSimSarsaStateData* FindStateByPackedKey(int32 PackedKey);
	FDSimSarsaStateData* FindOrAddState(const FDSimSarsaStateKey& Key);

	FDSimSarsaActionValue* FindActionValue(FDSimSarsaStateData& State, EBotAction Action);
	const FDSimSarsaActionValue* FindActionValue(const FDSimSarsaStateData& State, EBotAction Action) const;

	EBotAction SelectActionEpsilonGreedy(const FDSimSarsaStateData& State) const;
	float GetQValueForAction(const FDSimSarsaStateData& State, EBotAction Action) const;

	void ApplySarsaUpdate(
		int32 StatePackedKey,
		EBotAction Action,
		float Reward,
		const FDSimSarsaStateData* NextState,
		EBotAction NextAction,
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
	FDSimSarsaTableData QTable;

	UPROPERTY()
	TMap<int32, int32> StateIndexByPackedKey;

	UPROPERTY()
	TObjectPtr<UDSimSarsaAlgorithmConfig> SarsaConfig;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float Alpha = 0.3f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float Gamma = 0.95f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float Epsilon = 0.2f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float EpsilonMin = 0.05f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float EpsilonDecay = 0.99f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	int32 NumSections = 100;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	int32 NumLanes = 1;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float LaneHalfWidth = 1500.0f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	bool bUse2DState = true;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	float InitialQValue = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "SARSA|Runtime")
	bool bCurrentEpisodeFinalized = false;

	FVector StartPosition = FVector::ZeroVector;
	FVector GoalPosition = FVector::ZeroVector;
	float PathLength = 1.0f;

	bool bHasLastTransition = false;
	int32 LastStatePackedKey = INDEX_NONE;
	EBotAction LastAction = EBotAction::None;

	// Accumulates reward between the previous action and the next state-action observation.
	float PendingReward = 0.0f;

	FString DefaultSaveFileName = TEXT("SARSA_Data.json");
};
