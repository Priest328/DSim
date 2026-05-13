#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSimSarsaAlgorithmConfig.generated.h"

/**
 * Config for tabular SARSA.
 *
 * SARSA is an on-policy TD algorithm:
 * Q(s,a) = Q(s,a) + Alpha * (Reward + Gamma * Q(s',a') - Q(s,a))
 *
 * Difference from Q-Learning:
 * - Q-Learning uses max Q(s',a') for the next state.
 * - SARSA uses the actually selected next action a'.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class DSIM_API UDSimSarsaAlgorithmConfig : public UDSimBotTrainingAlgorithmConfig
{
	GENERATED_BODY()

public:
	// Learning rate. Higher values adapt faster but can make learning unstable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LearningRate = 0.3f;

	// Discount factor. Higher values make the agent consider future rewards more strongly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DiscountFactor = 0.95f;

	// Initial exploration probability. Higher values make the agent try more random actions at the beginning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonStart = 0.2f;

	// Minimum exploration probability. Keeps a small amount of exploration during long training.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonMin = 0.05f;

	// Epsilon multiplier after every episode. Lower values reduce exploration faster.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonDecay = 0.99f;

	// Number of progress sections between start and goal. More sections improve precision but increase table size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|State", meta = (ClampMin = "1"))
	int32 HorizontalSections = 100;

	// Number of side lanes for 2D state representation. More lanes give better spatial awareness but require more training.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|State", meta = (ClampMin = "1"))
	int32 VerticalLanes = 5;

	// Half-width of the movement corridor used for lane calculation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|State", meta = (ClampMin = "1.0"))
	float LaneHalfWidth = 1500.0f;

	// Initial Q-value for newly created state-action pairs. Positive values encourage optimistic exploration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Q Table")
	float InitialQValue = 0.0f;

	// Saves Q-table automatically after each episode. Useful for debugging, but can add disk I/O overhead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|IO")
	bool bAutoSaveAfterEpisode = false;

	// Default file name when SimulationManager does not provide an output path.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|IO")
	FString SaveFileName = TEXT("SARSA_Data.json");

	// Reward added when the bot reaches the goal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Terminal Rewards")
	float GoalReachedTerminalReward = 1.0f;

	// Reward added when the bot is killed by the drone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Terminal Rewards")
	float BotKilledTerminalReward = -1.0f;

	// Reward added when the drone crashes. Positive value means the bot benefits from surviving the drone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Terminal Rewards")
	float DroneCrashedTerminalReward = 0.4f;

	// Reward added when the episode ends by timeout. Usually should be neutral or negative.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Terminal Rewards")
	float TimeoutTerminalReward = -0.2f;

	// Optional Q-value clipping. Helps prevent unstable Q-values in early tests.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Stability")
	bool bClampQValues = false;

	// Minimum allowed Q-value when clipping is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Stability", meta = (EditCondition = "bClampQValues"))
	float MinQValue = -10.0f;

	// Maximum allowed Q-value when clipping is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SARSA|Stability", meta = (EditCondition = "bClampQValues"))
	float MaxQValue = 10.0f;
};
