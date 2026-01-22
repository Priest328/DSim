// Fill out your copyright notice in the Description page of Project Settings.

#include "DSim/AI/Services/BTService_SelectPPOAction.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/ML/DSimReinforcementLearningComp.h"
#include "DSim/Character/DSimCharacter.h"

UBTService_SelectPPOAction::UBTService_SelectPPOAction()
{
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	NodeName = "Select PPO Action";
}

void UBTService_SelectPPOAction::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	ADSimCharacterAIController* AICon = Cast<ADSimCharacterAIController>(OwnerComp.GetAIOwner());
	ADSimCharacter* Bot = Cast<ADSimCharacter>(AICon ? AICon->GetPawn() : nullptr);
	if (!IsValid(Bot))
	{
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	const FVector Goal = BB->GetValueAsVector(AIBlackboardKeys::GoalLocation);

	UDSimReinforcementLearningComp* RL = IsValid(AICon) ? AICon->RLComp : nullptr;
	if (!IsValid(RL))
	{
		return;
	}

	// Даємо RL-компоненту актуальну ціль (важливо, якщо ціль/маркер рухається або змінюється в BT)
	if (!Goal.IsNearlyZero())
	{
		RL->SetGoalPosition(Goal);
	}

	const EBotAction Action = RL->RequestAction();
	BB->SetValueAsEnum(AIBlackboardKeys::CurrentBotAction, static_cast<uint8>(Action));
}
