#include "DSimTDQLearningAlgorithmComponent.h"

#include "DSimTDQLearningAlgorithmConfig.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"

#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UDSimTDQLearningAlgorithmComponent::UDSimTDQLearningAlgorithmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimTDQLearningAlgorithmComponent::InitializeAlgorithm(
	const FDSimAlgorithmRuntimeContext& InContext,
	UDSimBotTrainingAlgorithmConfig* InConfig
)
{
	Super::InitializeAlgorithm(InContext, InConfig);

	TDConfig = Cast<UDSimTDQLearningAlgorithmConfig>(InConfig);

	if (TDConfig)
	{
		Alpha = TDConfig->LearningRate;
		Gamma = TDConfig->DiscountFactor;
		Epsilon = TDConfig->EpsilonStart;
		EpsilonMin = TDConfig->EpsilonMin;
		EpsilonDecay = TDConfig->EpsilonDecay;

		NumSections = FMath::Max(1, TDConfig->HorizontalSections);
		NumLanes = FMath::Max(1, TDConfig->VerticalLanes);
		LaneHalfWidth = FMath::Max(1.0f, TDConfig->LaneHalfWidth);

		InitialQValue = TDConfig->InitialQValue;
		DefaultSaveFileName = TDConfig->SaveFileName;
	}

	switch (InContext.StateRepresentationMode)
	{
	case EDSimStateRepresentationMode::OneD:
		bUse2DState = false;
		NumLanes = 1;
		break;

	case EDSimStateRepresentationMode::TwoD:
	case EDSimStateRepresentationMode::TwoDThreatAware:
	default:
		bUse2DState = true;
		NumLanes = FMath::Max(1, NumLanes);
		break;
	}

	ResolveOwnerReferences();

	if (OwnerActor)
	{
		StartPosition = OwnerActor->GetActorLocation();
	}

	if (GoalPosition.IsNearlyZero())
	{
		TArray<AActor*> Goals;
		UGameplayStatics::GetAllActorsOfClass(this, ADSimGoalActor::StaticClass(), Goals);

		if (!Goals.IsEmpty())
		{
			GoalPosition = Goals[0]->GetActorLocation();
		}
	}

	PathLength = FMath::Max(10.0f, FVector::Dist(StartPosition, GoalPosition));

	if (!InContext.InitialTrainingFile.IsEmpty())
	{
		LoadTrainingData(InContext.InitialTrainingFile);
	}
	else
	{
		InitializeEmptyTable();
	}

	bCurrentEpisodeFinalized = false;
	bHasLastTransition = false;
	PendingReward = 0.0f;
}

void UDSimTDQLearningAlgorithmComponent::StartEpisode(int32 EpisodeId)
{
	Super::StartEpisode(EpisodeId);

	ResolveOwnerReferences();

	if (OwnerActor)
	{
		StartPosition = OwnerActor->GetActorLocation();
		PathLength = FMath::Max(10.0f, FVector::Dist(StartPosition, GoalPosition));
	}

	bCurrentEpisodeFinalized = false;
	bHasLastTransition = false;
	LastStatePackedKey = INDEX_NONE;
	LastAction = EBotAction::None;
	PendingReward = 0.0f;
}

void UDSimTDQLearningAlgorithmComponent::EndEpisode(EDSimEpisodeFinishReason FinishReason)
{
	Super::EndEpisode(FinishReason);

	if (bCurrentEpisodeFinalized)
	{
		return;
	}

	const float TerminalReward = GetTerminalRewardForFinishReason(FinishReason);
	FlushPendingTransitionAsTerminal(TerminalReward);

	DecayEpsilon();

	if (TDConfig && TDConfig->bAutoSaveAfterEpisode)
	{
		SaveTrainingData(TEXT(""));
	}

	bCurrentEpisodeFinalized = true;
	RecordTrainingReward(TerminalReward);
}

EBotAction UDSimTDQLearningAlgorithmComponent::RequestTrainingAction()
{
	ResolveOwnerReferences();

	if (!OwnerActor)
	{
		return EBotAction::None;
	}

	const FDSimTDQStateKey CurrentKey = DiscretizeCurrentState();
	FDSimTDQStateData* CurrentState = FindOrAddState(CurrentKey);

	if (!CurrentState)
	{
		return EBotAction::None;
	}

	// TD update is performed when the next state becomes known.
	if (bHasLastTransition)
	{
		ApplyTDUpdate(
			LastStatePackedKey,
			LastAction,
			PendingReward,
			CurrentState,
			false
		);

		PendingReward = 0.0f;
	}

	const EBotAction SelectedAction = SelectActionEpsilonGreedy(*CurrentState);

	LastStatePackedKey = CurrentKey.PackedKey;
	LastAction = SelectedAction;
	bHasLastTransition = true;

	return SelectedAction;
}

void UDSimTDQLearningAlgorithmComponent::AddTrainingReward(float Reward)
{
	// Reward is accumulated until the next state is observed.
	PendingReward += Reward;
}

void UDSimTDQLearningAlgorithmComponent::SetGoalPosition(const FVector& NewGoalPosition)
{
	GoalPosition = NewGoalPosition;

	if (OwnerActor)
	{
		StartPosition = StartPosition.IsNearlyZero() ? OwnerActor->GetActorLocation() : StartPosition;
		PathLength = FMath::Max(10.0f, FVector::Dist(StartPosition, GoalPosition));
	}
}

bool UDSimTDQLearningAlgorithmComponent::LoadTrainingData(const FString& FileName)
{
	const FString Path = ResolveTrainingDataPath(FileName);

	FString Json;
	if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
	{
		InitializeEmptyTable();
		return false;
	}

	FDSimTDQTableData LoadedTable;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &LoadedTable, 0, 0))
	{
		InitializeEmptyTable();
		return false;
	}

	QTable = LoadedTable;

	NumSections = FMath::Max(1, QTable.NumSections);
	NumLanes = FMath::Max(1, QTable.NumLanes);
	LaneHalfWidth = FMath::Max(1.0f, QTable.LaneHalfWidth);

	if (!bUse2DState)
	{
		NumLanes = 1;
		QTable.NumLanes = 1;
	}

	RebuildStateIndex();
	return true;
}

bool UDSimTDQLearningAlgorithmComponent::SaveTrainingData(const FString& FileName)
{
	QTable.NumSections = FMath::Max(1, NumSections);
	QTable.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	QTable.LaneHalfWidth = FMath::Max(1.0f, LaneHalfWidth);

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(QTable, Json))
	{
		return false;
	}

	const FString Path = ResolveTrainingDataPath(FileName);
	const FString Directory = FPaths::GetPath(Path);

	IFileManager::Get().MakeDirectory(*Directory, true);

	return FFileHelper::SaveStringToFile(Json, *Path);
}

FString UDSimTDQLearningAlgorithmComponent::GetAlgorithmName() const
{
	return bUse2DState ? TEXT("TD_Q_Learning_2D") : TEXT("TD_Q_Learning_1D");
}

void UDSimTDQLearningAlgorithmComponent::ResolveOwnerReferences()
{
	AIController = Cast<ADSimCharacterAIController>(GetOwner());

	if (AIController)
	{
		OwnerActor = Cast<ADSimCharacter>(AIController->GetPawn());
		return;
	}

	OwnerActor = Cast<ADSimCharacter>(GetOwner());
}

void UDSimTDQLearningAlgorithmComponent::InitializeEmptyTable()
{
	QTable.NumSections = FMath::Max(1, NumSections);
	QTable.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	QTable.LaneHalfWidth = FMath::Max(1.0f, LaneHalfWidth);
	QTable.AllStates.Empty();

	StateIndexByPackedKey.Empty();
}

void UDSimTDQLearningAlgorithmComponent::RebuildStateIndex()
{
	StateIndexByPackedKey.Empty();

	for (int32 i = 0; i < QTable.AllStates.Num(); ++i)
	{
		StateIndexByPackedKey.Add(QTable.AllStates[i].Key.PackedKey, i);
	}
}

FDSimTDQStateKey UDSimTDQLearningAlgorithmComponent::DiscretizeCurrentState() const
{
	FDSimTDQStateKey Key;

	if (!OwnerActor)
	{
		return Key;
	}

	const int32 SafeSections = FMath::Max(1, NumSections);
	const int32 SafeLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	const float SafePathLength = FMath::Max(10.0f, PathLength);

	const FVector CurrentPosition = OwnerActor->GetActorLocation();

	const float Progress = 1.0f - FVector::Dist(CurrentPosition, GoalPosition) / SafePathLength;

	Key.SectionIndex = FMath::Clamp(
		FMath::RoundToInt(Progress * static_cast<float>(SafeSections)),
		0,
		SafeSections
	);

	Key.SectionKey = static_cast<float>(Key.SectionIndex) / static_cast<float>(SafeSections);

	if (!bUse2DState)
	{
		Key.LaneIndex = 0;
		Key.RebuildPackedKey(1);
		return Key;
	}

	const FVector Direction = (GoalPosition - StartPosition).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();

	const float SafeHalfWidth = FMath::Max(1.0f, LaneHalfWidth);
	const float LaneWidth = (2.0f * SafeHalfWidth) / static_cast<float>(SafeLanes);

	const float Offset = FVector::DotProduct(CurrentPosition - StartPosition, Right);
	const float NormalizedLane = (Offset + SafeHalfWidth) / FMath::Max(1.0f, LaneWidth);

	Key.LaneIndex = FMath::Clamp(FMath::FloorToInt(NormalizedLane), 0, SafeLanes - 1);
	Key.RebuildPackedKey(SafeLanes);

	return Key;
}

FDSimTDQStateData* UDSimTDQLearningAlgorithmComponent::FindStateByPackedKey(int32 PackedKey)
{
	const int32* FoundIndex = StateIndexByPackedKey.Find(PackedKey);

	if (!FoundIndex)
	{
		return nullptr;
	}

	return QTable.AllStates.IsValidIndex(*FoundIndex)
		? &QTable.AllStates[*FoundIndex]
		: nullptr;
}

FDSimTDQStateData* UDSimTDQLearningAlgorithmComponent::FindOrAddState(const FDSimTDQStateKey& Key)
{
	if (FDSimTDQStateData* ExistingState = FindStateByPackedKey(Key.PackedKey))
	{
		return ExistingState;
	}

	FDSimTDQStateData NewState;
	NewState.Key = Key;

	const EBotAction DefaultActions[] =
	{
		EBotAction::TowardGoal,
		EBotAction::TowardCover,
		EBotAction::RandomMove
	};

	for (const EBotAction Action : DefaultActions)
	{
		FDSimTDQActionValue ActionValue;
		ActionValue.BotAction = Action;
		ActionValue.QValue = InitialQValue;
		NewState.Actions.Add(ActionValue);
	}

	const int32 NewIndex = QTable.AllStates.Add(NewState);
	StateIndexByPackedKey.Add(Key.PackedKey, NewIndex);

	return &QTable.AllStates[NewIndex];
}

FDSimTDQActionValue* UDSimTDQLearningAlgorithmComponent::FindActionValue(
	FDSimTDQStateData& State,
	EBotAction Action
)
{
	FDSimTDQActionValue* ActionValue = State.FindAction(Action);

	if (ActionValue)
	{
		return ActionValue;
	}

	FDSimTDQActionValue NewActionValue;
	NewActionValue.BotAction = Action;
	NewActionValue.QValue = InitialQValue;

	const int32 NewIndex = State.Actions.Add(NewActionValue);
	return State.Actions.IsValidIndex(NewIndex) ? &State.Actions[NewIndex] : nullptr;
}

EBotAction UDSimTDQLearningAlgorithmComponent::SelectActionEpsilonGreedy(
	const FDSimTDQStateData& State
) const
{
	if (State.Actions.Num() == 0)
	{
		return EBotAction::None;
	}

	if (FMath::FRand() < Epsilon)
	{
		const int32 RandomIndex = FMath::RandRange(0, State.Actions.Num() - 1);
		return State.Actions[RandomIndex].BotAction;
	}

	float BestQ = -FLT_MAX;
	EBotAction BestAction = EBotAction::None;

	for (const FDSimTDQActionValue& ActionValue : State.Actions)
	{
		if (ActionValue.QValue > BestQ)
		{
			BestQ = ActionValue.QValue;
			BestAction = ActionValue.BotAction;
		}
	}

	return BestAction;
}

float UDSimTDQLearningAlgorithmComponent::GetMaxQValue(const FDSimTDQStateData& State) const
{
	if (State.Actions.Num() == 0)
	{
		return 0.0f;
	}

	float MaxQ = -FLT_MAX;

	for (const FDSimTDQActionValue& ActionValue : State.Actions)
	{
		MaxQ = FMath::Max(MaxQ, ActionValue.QValue);
	}

	return MaxQ == -FLT_MAX ? 0.0f : MaxQ;
}

void UDSimTDQLearningAlgorithmComponent::ApplyTDUpdate(
	int32 StatePackedKey,
	EBotAction Action,
	float Reward,
	const FDSimTDQStateData* NextState,
	bool bTerminal
)
{
	FDSimTDQStateData* State = FindStateByPackedKey(StatePackedKey);

	if (!State)
	{
		return;
	}

	FDSimTDQActionValue* ActionValue = FindActionValue(*State, Action);

	if (!ActionValue)
	{
		return;
	}

	const float OldQ = ActionValue->QValue;
	const float NextMaxQ = (!bTerminal && NextState) ? GetMaxQValue(*NextState) : 0.0f;

	const float TDTarget = Reward + Gamma * NextMaxQ;
	const float TDError = TDTarget - OldQ;

	float NewQ = OldQ + Alpha * TDError;

	if (TDConfig && TDConfig->bClampQValues)
	{
		NewQ = FMath::Clamp(NewQ, TDConfig->MinQValue, TDConfig->MaxQValue);
	}

	ActionValue->QValue = NewQ;
}

void UDSimTDQLearningAlgorithmComponent::FlushPendingTransitionAsTerminal(float TerminalReward)
{
	if (!bHasLastTransition)
	{
		PendingReward = 0.0f;
		return;
	}

	const float FinalReward = PendingReward + TerminalReward;

	ApplyTDUpdate(
		LastStatePackedKey,
		LastAction,
		FinalReward,
		nullptr,
		true
	);

	bHasLastTransition = false;
	LastStatePackedKey = INDEX_NONE;
	LastAction = EBotAction::None;
	PendingReward = 0.0f;
}

void UDSimTDQLearningAlgorithmComponent::DecayEpsilon()
{
	Epsilon = FMath::Max(EpsilonMin, Epsilon * EpsilonDecay);
}

float UDSimTDQLearningAlgorithmComponent::GetTerminalRewardForFinishReason(
	EDSimEpisodeFinishReason FinishReason
) const
{
	if (!TDConfig)
	{
		switch (FinishReason)
		{
		case EDSimEpisodeFinishReason::GoalReached:
			return 1.0f;

		case EDSimEpisodeFinishReason::BotKilledByDrone:
			return -1.0f;

		case EDSimEpisodeFinishReason::DroneCrashed:
			return 0.4f;

		case EDSimEpisodeFinishReason::Timeout:
			return -0.2f;

		default:
			return 0.0f;
		}
	}

	switch (FinishReason)
	{
	case EDSimEpisodeFinishReason::GoalReached:
		return TDConfig->GoalReachedTerminalReward;

	case EDSimEpisodeFinishReason::BotKilledByDrone:
		return TDConfig->BotKilledTerminalReward;

	case EDSimEpisodeFinishReason::DroneCrashed:
		return TDConfig->DroneCrashedTerminalReward;

	case EDSimEpisodeFinishReason::Timeout:
		return TDConfig->TimeoutTerminalReward;

	case EDSimEpisodeFinishReason::StoppedManually:
	case EDSimEpisodeFinishReason::Unknown:
	default:
		return 0.0f;
	}
}

FString UDSimTDQLearningAlgorithmComponent::ResolveTrainingDataPath(const FString& FileName) const
{
	const FString EffectiveFileName = FileName.IsEmpty() ? DefaultSaveFileName : FileName;

	if (FPaths::IsRelative(EffectiveFileName))
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames/");
		IFileManager::Get().MakeDirectory(*Dir, true);
		return Dir / EffectiveFileName;
	}

	return EffectiveFileName;
}