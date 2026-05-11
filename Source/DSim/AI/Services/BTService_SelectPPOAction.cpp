// Fill out your copyright notice in the Description page of Project Settings.

#include "DSim/AI/Services/BTService_SelectPPOAction.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSim/Character/DSimCharacter.h"

UBTService_SelectPPOAction::UBTService_SelectPPOAction()
{
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	NodeName = "Select Bot Training Action";
}

void UBTService_SelectPPOAction::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds
)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	ADSimCharacterAIController* AICon = Cast<ADSimCharacterAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(AICon))
	{
		return;
	}

	ADSimCharacter* Bot = Cast<ADSimCharacter>(AICon->GetPawn());
	if (!IsValid(Bot))
	{
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BB))
	{
		return;
	}

	const FVector Goal = BB->GetValueAsVector(AIBlackboardKeys::GoalLocation);

	UDSimBotTrainingAlgorithmComponent* Algorithm =
		Bot->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();

	if (!IsValid(Algorithm))
	{
		Algorithm = AICon->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();
	}

	if (!IsValid(Algorithm))
	{
		return;
	}

	if (!Goal.IsNearlyZero())
	{
		Algorithm->SetGoalPosition(Goal);
	}

	const EBotAction Action = Algorithm->RequestTrainingAction();
	BB->SetValueAsEnum(AIBlackboardKeys::CurrentBotAction, static_cast<uint8>(Action));
}