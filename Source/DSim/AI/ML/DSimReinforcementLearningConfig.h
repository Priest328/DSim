#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSim/AI/Training/DSimTrainingTypes.h"
#include "DSimReinforcementLearningConfig.generated.h"

/**
 * Configuration object for UDSimReinforcementLearningComp.
 *
 * The SimulationManager can create this object inline for every Drone/Bot pair,
 * so every arena can run the same RL component with different parameters:
 * 1D state, 2D state, threat-aware state, different epsilon, alpha, gamma, etc.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class DSIM_API UDSimReinforcementLearningConfig : public UDSimBotTrainingAlgorithmConfig
{
	GENERATED_BODY()

public:
	// -----------------------------
	// State representation
	// -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|State")
	EDSimStateRepresentationMode StateRepresentationMode = EDSimStateRepresentationMode::TwoD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|State", meta = (ClampMin = "1"))
	int32 NumSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|State", meta = (ClampMin = "1"))
	int32 NumLanes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|State", meta = (ClampMin = "1.0"))
	float LaneHalfWidth = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|State")
	bool bUseThreatFeatures = false;

	// -----------------------------
	// Learning parameters
	// -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LearningRate = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DiscountFactor = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonStart = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonMin = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpsilonDecay = 0.99f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Replay", meta = (ClampMin = "1"))
	int32 ReplayBufferSize = 100;

	// -----------------------------
	// Rewards
	// -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Reward")
	float GoalReachedTerminalReward = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Reward")
	float BotKilledTerminalReward = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Reward")
	float DroneCrashedTerminalReward = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Reward")
	float TimeoutTerminalReward = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Reward")
	float CriticalDroneDistance = 900.f;

	// -----------------------------
	// Debug
	// -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Debug")
	bool bDrawDebug = false;

	// -----------------------------
	// Legacy IO
	// SimulationManager should normally provide unique output paths.
	// These names are kept for manual tests and legacy compatibility.
	// -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Legacy IO")
	FString LegacySaveFileName1D = TEXT("RLData_1D.json");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL|Legacy IO")
	FString LegacySaveFileName2D = TEXT("RLData_2D.json");
};
