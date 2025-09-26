// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_GetRandomForwardPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "DSim/AI/DSimCharacterAIController.h"

UBTTask_GetRandomForwardPoint::UBTTask_GetRandomForwardPoint()
{
	NodeName = "Get Random Forward Point";
}

EBTNodeResult::Type UBTTask_GetRandomForwardPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return EBTNodeResult::Failed;

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* AIPawn = AIController ? AIController->GetPawn() : nullptr;
	if (!AIPawn)
		return EBTNodeResult::Failed;

	FVector AIPos = AIPawn->GetActorLocation();
	FVector GoalPos = Blackboard->GetValueAsVector(AIBlackboardKeys::GoalLocation);

	// Get normalized direction to goal
	FVector ToGoal = (GoalPos - AIPos).GetSafeNormal();

	// Generate random angle within +/-90 degrees (π/2 radians) around the direction
	const float RandomAngleRad = FMath::FRandRange(-PI / 2, PI / 2);

	// Rotate direction around Z axis by random angle
	FVector RotatedDirection = ToGoal.RotateAngleAxis(FMath::RadiansToDegrees(RandomAngleRad), FVector::UpVector);

	// Random length within radius
	float Length = FMath::FRandRange(Radius * 0.3f, Radius);

	FVector RandomPoint = AIPos + RotatedDirection * Length;

	Blackboard->SetValueAsVector(AIBlackboardKeys::MoveLocation, RandomPoint);

	return EBTNodeResult::Succeeded;
}