// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTaskNode_GetForwardGoalDisLoc.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "DSim/AI/DSimCharacterAIController.h"

UBTTaskNode_GetForwardGoalDisLoc::UBTTaskNode_GetForwardGoalDisLoc()
{
	NodeName = "Get Forward Goal Location";
}

EBTNodeResult::Type UBTTaskNode_GetForwardGoalDisLoc::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
		return EBTNodeResult::Failed;

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* AIPawn = AIController ? AIController->GetPawn() : nullptr;
	if (!AIPawn)
		return EBTNodeResult::Failed;

	const FVector AIPos = AIPawn->GetActorLocation();
	const FVector GoalPos = BlackboardComp->GetValueAsVector(AIBlackboardKeys::GoalLocation);

	const FVector Direction = (GoalPos - AIPos).GetSafeNormal();
	const FVector ForwardTarget = AIPos + Direction * ForwardDistance;

	BlackboardComp->SetValueAsVector(AIBlackboardKeys::MoveLocation, ForwardTarget);

	return EBTNodeResult::Succeeded;
}