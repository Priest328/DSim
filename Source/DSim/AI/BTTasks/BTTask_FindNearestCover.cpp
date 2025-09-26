// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/AI/BTTasks/BTTask_FindNearestCover.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SphereComponent.h"
#include "DSim/Actors/DSimCoverActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"

EBTNodeResult::Type UBTTask_FindNearestCover::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	ADSimCharacter* Bot = Cast<ADSimCharacter>(AICon->GetPawn());
	if (!Bot || !Bot->CoverDetectionSphere) return EBTNodeResult::Failed;

	TArray<AActor*> Covers;
	Bot->CoverDetectionSphere->GetOverlappingActors(Covers, ADSimCoverActor::StaticClass());

	if (Covers.Num() == 0)
	{
		Cast<ADSimCharacterAIController>(AICon)->RLComp->ApplyReward(-0.1, false);
		return EBTNodeResult::Failed;
	}

	const FVector BotLocation = Bot->GetActorLocation();
	const FVector Goal = OwnerComp.GetBlackboardComponent()->GetValueAsVector(AIBlackboardKeys::GoalLocation);

	// Step 1: Find closest cover to bot
	AActor* BestCover = nullptr;
	float ClosestDistance = FLT_MAX;

	for (AActor* Cover : Covers)
	{
		const float DistanceToBot = FVector::Dist(BotLocation, Cover->GetActorLocation());
		if (DistanceToBot < ClosestDistance)
		{
			ClosestDistance = DistanceToBot;
			BestCover = Cover;
		}
	}

	if (!BestCover)
	{
		return EBTNodeResult::Failed;
	}

	const FVector BestCoverLoc = BestCover->GetActorLocation();
	const float BestCoverToGoal = FVector::Dist(BestCoverLoc, Goal);

	// Step 2: Look for better cover closer to goal but further from bot
	for (AActor* Cover : Covers)
	{
		if (Cover == BestCover) continue;

		const FVector CoverLoc = Cover->GetActorLocation();
		const float DistanceToBot = FVector::Dist(BotLocation, CoverLoc);
		const float DistanceToGoal = FVector::Dist(CoverLoc, Goal);

		// Accept only covers further from bot than BestCover but closer to goal
		if (DistanceToBot > ClosestDistance && DistanceToGoal < BestCoverToGoal)
		{
			BestCover = Cover;
			ClosestDistance = DistanceToBot; // update for additional comparisons if needed
		}
	}

	OwnerComp.GetBlackboardComponent()->SetValueAsVector(AIBlackboardKeys::NearestCover, BestCover->GetActorLocation());
	return EBTNodeResult::Succeeded;
}