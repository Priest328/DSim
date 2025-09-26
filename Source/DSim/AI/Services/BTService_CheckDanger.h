// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_CheckDanger.generated.h"

/**
 * 
 */
UCLASS()
class DSIM_API UBTService_CheckDanger : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_CheckDanger();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInDangerKey;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float DangerDistance = 1200.0f;
};
