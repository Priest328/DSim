#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTaskNode_GetForwardGoalDisLoc.generated.h"

UCLASS()
class DSIM_API UBTTaskNode_GetForwardGoalDisLoc : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTaskNode_GetForwardGoalDisLoc();

	/** Distance forward from AI toward GoalActor */
	UPROPERTY(EditAnywhere, Category = "AI")
	float ForwardDistance = 500.0f;


protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};