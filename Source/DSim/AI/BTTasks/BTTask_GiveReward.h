#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GiveReward.generated.h"

/**
 * Gives an intermediate reward to the active training algorithm.
 *
 * This task should be used for non-terminal feedback:
 * - successful movement
 * - failed movement
 * - reached cover
 * - invalid action
 *
 * Terminal rewards should be handled by SimulationManager:
 * - GoalReached
 * - BotKilledByDrone
 * - DroneCrashed
 * - Timeout
 */
UCLASS()
class DSIM_API UBTTask_GiveReward : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_GiveReward();

	// Reward value passed to the current training algorithm.
	UPROPERTY(EditAnywhere, Category = "Reward")
	float RewardAmount = 0.0f;

	// If true, this task returns Failed after giving reward.
	// Useful when you want to penalize a branch and force BT fallback.
	UPROPERTY(EditAnywhere, Category = "Reward")
	bool bFailEvenIfSucceed = false;

	// If true, missing algorithm component makes this task fail.
	// Usually false is safer, because reward task should not break the BT in non-training mode.
	UPROPERTY(EditAnywhere, Category = "Reward")
	bool bFailIfNoAlgorithm = false;

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory
	) override;
};