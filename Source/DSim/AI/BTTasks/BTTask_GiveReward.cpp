// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/AI/BTTasks/BTTask_GiveReward.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/DSimDebugComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"

EBTNodeResult::Type UBTTask_GiveReward::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ADSimCharacterAIController* Controller = Cast<ADSimCharacterAIController>(OwnerComp.GetOwner());
	if (!IsValid(Controller)) return EBTNodeResult::Failed;

	Controller->RLComp->ApplyReward(RewardAmount, false);
	
	return EBTNodeResult::Succeeded;
}
