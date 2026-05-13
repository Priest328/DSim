#include "DSim/AI/BTTasks/BTTask_GiveReward.h"

#include "AIController.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSim/Character/DSimCharacter.h"

UBTTask_GiveReward::UBTTask_GiveReward()
{
	NodeName = TEXT("Give Training Reward");
}

EBTNodeResult::Type UBTTask_GiveReward::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory
)
{
	ADSimCharacterAIController* Controller = Cast<ADSimCharacterAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(Controller))
	{
		return bFailIfNoAlgorithm ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
	}

	ADSimCharacter* Bot = Cast<ADSimCharacter>(Controller->GetPawn());

	UDSimBotTrainingAlgorithmComponent* Algorithm = nullptr;

	if (IsValid(Bot))
	{
		Algorithm = Bot->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();
	}

	if (!IsValid(Algorithm))
	{
		Algorithm = Controller->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();
	}

	if (!IsValid(Algorithm))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_GiveReward] No training algorithm found."));
		return bFailIfNoAlgorithm ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
	}

	Algorithm->AddTrainingReward(RewardAmount);
	Algorithm->RecordTrainingReward(RewardAmount);

	return bFailEvenIfSucceed ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
}