// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GiveReward.generated.h"

/**
 * 
 */
UCLASS()
class DSIM_API UBTTask_GiveReward : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float RewardAmount = 0.0f;
	
	UPROPERTY(EditAnywhere)
	bool bFailEvenIfSucceed = false;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
