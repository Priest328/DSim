#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimTDQLearningAlgorithmConfig.generated.h"

/**
 * Config for tabular TD Q-Learning.
 *
 * TD Q-Learning updates Q-values after each transition:
 * Q(s,a) = Q(s,a) + Alpha * (Reward + Gamma * max_a Q(s',a) - Q(s,a))
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class DSIM_API UDSimTDQLearningAlgorithmConfig : public UDSimBotTrainingAlgorithmConfig
{
	GENERATED_BODY()

public:
	// Learning rate. Higher values adapt faster, but may make Q-values unstable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LearningRate = 0.3f;

	// Discount factor. Higher values make the agent care more about future rewards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DiscountFactor = 0.95f;

	// Initial exploration probability. Higher values make the agent explore more at the beginning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonStart = 0.2f;

	// Minimum exploration probability. Prevents the agent from becoming fully deterministic.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonMin = 0.05f;

	// Epsilon multiplier after each episode. Lower values reduce exploration faster.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonDecay = 0.99f;

	// Number of progress sections between start and goal. More sections give more precise states but require more training.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|State", meta = (ClampMin = "1"))
	int32 HorizontalSections = 100;

	// Number of side lanes for 2D state representation. More lanes improve spatial precision but increase the Q-table size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|State", meta = (ClampMin = "1"))
	int32 VerticalLanes = 5;

	// Half-width of the lane grid around the start-goal direction. Larger values cover a wider movement corridor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|State", meta = (ClampMin = "1.0"))
	float LaneHalfWidth = 1500.0f;

	// Initial Q-value for newly created state-action pairs. Positive values encourage optimistic exploration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Q Table")
	float InitialQValue = 0.0f;

	// Saves Q-table automatically after each episode. Useful for long experiments, but may add disk I/O overhead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|IO")
	bool bAutoSaveAfterEpisode = false;

	// Default file name when SimulationManager does not provide an output file.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|IO")
	FString SaveFileName = TEXT("TD_Q_Learning_Data.json");

	// Reward added when the bot reaches the goal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Terminal Rewards")
	float GoalReachedTerminalReward = 1.0f;

	// Reward added when the bot is killed by the drone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Terminal Rewards")
	float BotKilledTerminalReward = -1.0f;

	// Reward added when the drone crashes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Terminal Rewards")
	float DroneCrashedTerminalReward = 0.4f;

	// Reward added when the episode ends by timeout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Terminal Rewards")
	float TimeoutTerminalReward = -0.2f;

	// Optional Q-value clipping. Helps prevent large unstable Q-values during early experiments.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Stability")
	bool bClampQValues = false;

	// Minimum allowed Q-value when clipping is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Stability", meta = (EditCondition = "bClampQValues"))
	float MinQValue = -10.0f;

	// Maximum allowed Q-value when clipping is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD Q-Learning|Stability", meta = (EditCondition = "bClampQValues"))
	float MaxQValue = 10.0f;
};