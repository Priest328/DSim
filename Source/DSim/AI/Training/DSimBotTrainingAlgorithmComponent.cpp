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

	CurrentEpisodeSummary.EpisodeId = EpisodeId;
	CurrentEpisodeSummary.AlgorithmName = GetAlgorithmName();
	CurrentEpisodeSummary.FinishReason = EDSimEpisodeFinishReason::Unknown;
	CurrentEpisodeSummary.TotalReward = 0.0f;
	CurrentEpisodeSummary.EpisodeDuration = 0.0f;
	CurrentEpisodeSummary.TowardGoalCount = 0;
	CurrentEpisodeSummary.TowardCoverCount = 0;
	CurrentEpisodeSummary.RandomMoveCount = 0;

	EpisodeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UDSimBotTrainingAlgorithmComponent::EndEpisode(EDSimEpisodeFinishReason FinishReason)
{
	CurrentEpisodeSummary.FinishReason = FinishReason;

	if (GetWorld())
	{
		CurrentEpisodeSummary.EpisodeDuration =
			GetWorld()->GetTimeSeconds() - EpisodeStartTime;
	}
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

void UDSimBotTrainingAlgorithmComponent::RecordSelectedAction(EBotAction Action)
{
	switch (Action)
	{
	case EBotAction::TowardGoal:
		CurrentEpisodeSummary.TowardGoalCount++;
		break;

	case EBotAction::TowardCover:
		CurrentEpisodeSummary.TowardCoverCount++;
		break;

	case EBotAction::RandomMove:
		CurrentEpisodeSummary.RandomMoveCount++;
		break;

	default:
		break;
	}
}

void UDSimBotTrainingAlgorithmComponent::RecordTrainingReward(float Reward)
{
	CurrentEpisodeSummary.TotalReward += Reward;
}

void UDSimBotTrainingAlgorithmComponent::SetEpisodeSummaryContext(
	int32 InRunId,
	int32 InPairIndex,
	int32 InArenaId,
	EDSimStateRepresentationMode InStateRepresentationMode,
	const FString& InOutputTrainingFile
)
{
	CurrentEpisodeSummary.RunId = InRunId;
	CurrentEpisodeSummary.PairIndex = InPairIndex;
	CurrentEpisodeSummary.ArenaId = InArenaId;
	CurrentEpisodeSummary.StateRepresentationMode = InStateRepresentationMode;
	CurrentEpisodeSummary.OutputTrainingFile = InOutputTrainingFile;
}
