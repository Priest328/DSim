#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"

UDSimBotTrainingAlgorithmComponent::UDSimBotTrainingAlgorithmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimBotTrainingAlgorithmComponent::InitializeAlgorithm(
	const FDSimAlgorithmRuntimeContext& InContext,
	UDSimBotTrainingAlgorithmConfig* InConfig
)
{
	RuntimeContext = InContext;
	AlgorithmConfig = InConfig;

	if (!RuntimeContext.InitialTrainingFile.IsEmpty())
	{
		LoadTrainingData(RuntimeContext.InitialTrainingFile);
	}
}

void UDSimBotTrainingAlgorithmComponent::SetGoalPosition(const FVector& NewGoalPosition)
{
}

void UDSimBotTrainingAlgorithmComponent::StartEpisode(int32 EpisodeId)
{
	CurrentEpisodeId = EpisodeId;
}

void UDSimBotTrainingAlgorithmComponent::EndEpisode(EDSimEpisodeFinishReason FinishReason)
{
}

EBotAction UDSimBotTrainingAlgorithmComponent::RequestTrainingAction()
{
	return EBotAction::RandomMove;
}

void UDSimBotTrainingAlgorithmComponent::AddTrainingReward(float Reward)
{
}

bool UDSimBotTrainingAlgorithmComponent::LoadTrainingData(const FString& FileName)
{
	return false;
}

bool UDSimBotTrainingAlgorithmComponent::SaveTrainingData(const FString& FileName)
{
	return false;
}

FString UDSimBotTrainingAlgorithmComponent::GetAlgorithmName() const
{
	return GetClass()->GetName();
}