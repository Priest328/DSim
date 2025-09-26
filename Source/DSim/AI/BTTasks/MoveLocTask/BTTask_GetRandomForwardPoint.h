// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BTTask_GetRandomForwardPoint.generated.h"

UCLASS()
class DSIM_API UBTTask_GetRandomForwardPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_GetRandomForwardPoint();

	UPROPERTY(EditAnywhere, Category = "Random Movement")
	float Radius = 500.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};