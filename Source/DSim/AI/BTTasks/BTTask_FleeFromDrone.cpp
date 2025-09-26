// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/AI/BTTasks/BTTask_FleeFromDrone.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_FleeFromDrone::UBTTask_FleeFromDrone()
{
	NodeName = "Flee From Drone";
	TargetKey.SelectedKeyName = "TargetActor";
}

EBTNodeResult::Type UBTTask_FleeFromDrone::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!IsValid(&OwnerComp))
	{
		UE_LOG(LogTemp, Error, TEXT("OwnerComp is invalid"));
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController))
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!IsValid(Pawn))
	{
		return EBTNodeResult::Failed;
	}

	UObject* TargetObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);
	if (!IsValid(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	// Flee direction
	const FVector Direction = (Pawn->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal();
	const FVector FleeLocation = Pawn->GetActorLocation() + Direction * 600.0f;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	FNavLocation ResultLocation;
	if (NavSys && NavSys->GetRandomReachablePointInRadius(FleeLocation, 200.0f, ResultLocation))
	{
		AIController->MoveToLocation(ResultLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}