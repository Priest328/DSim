#include "DSim/AI/ML/DSimSarsaAlgorithmComponent.h"

#include "DSim/AI/ML/DSimSarsaAlgorithmConfig.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UDSimSarsaAlgorithmComponent::UDSimSarsaAlgorithmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimSarsaAlgorithmComponent::InitializeAlgorithm(
	const FDSimAlgorithmRuntimeContext& InContext,
	UDSimBotTrainingAlgorithmConfig* InConfig
)
{
	Super::InitializeAlgorithm(InContext, InConfig);

	SarsaConfig = Cast<UDSimSarsaAlgorithmConfig>(InConfig);

	if (SarsaConfig)
	{
		Alpha = SarsaConfig->LearningRate;
		Gamma = SarsaConfig->DiscountFactor;
		Epsilon = SarsaConfig->EpsilonStart;
		EpsilonMin = SarsaConfig->EpsilonMin;
		EpsilonDecay = SarsaConfig->EpsilonDecay;

		NumSections = FMath::Max(1, SarsaConfig->HorizontalSections);
		NumLanes = FMath::Max(1, SarsaConfig->VerticalLanes);
		LaneHalfWidth = FMath::Max(1.0f, SarsaConfig->LaneHalfWidth);

		InitialQValue = SarsaConfig->InitialQValue;
		DefaultSaveFileName = SarsaConfig->SaveFileName;
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
		UGameplayStatics::GetAllActorsOfClassWithTag(this, ADSimGoalActor::StaticClass(), OwnerActor->BotEnvironmentTag,
		                                             Goals);

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

void UDSimSarsaAlgorithmComponent::StartEpisode(int32 EpisodeId)
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

void UDSimSarsaAlgorithmComponent::EndEpisode(EDSimEpisodeFinishReason FinishReason)
{
	Super::EndEpisode(FinishReason);

	if (bCurrentEpisodeFinalized)
	{
		return;
	}

	const float TerminalReward = GetTerminalRewardForFinishReason(FinishReason);
	FlushPendingTransitionAsTerminal(TerminalReward);

	DecayEpsilon();

	if (SarsaConfig && SarsaConfig->bAutoSaveAfterEpisode)
	{
		SaveTrainingData(TEXT(""));
	}

	bCurrentEpisodeFinalized = true;
	RecordTrainingReward(TerminalReward);
}

EBotAction UDSimSarsaAlgorithmComponent::RequestTrainingAction()
{
	ResolveOwnerReferences();

	if (!OwnerActor)
	{
		return EBotAction::None;
	}

	const FDSimSarsaStateKey CurrentKey = DiscretizeCurrentState();
	FDSimSarsaStateData* CurrentState = FindOrAddState(CurrentKey);

	if (!CurrentState)
	{
		return EBotAction::None;
	}

	// SARSA must choose the next action first, because the update uses Q(s', a').
	const EBotAction SelectedAction = SelectActionEpsilonGreedy(*CurrentState);

	if (bHasLastTransition)
	{
		ApplySarsaUpdate(
			LastStatePackedKey,
			LastAction,
			PendingReward,
			CurrentState,
			SelectedAction,
			false
		);

		PendingReward = 0.0f;
	}

	LastStatePackedKey = CurrentKey.PackedKey;
	LastAction = SelectedAction;
	bHasLastTransition = true;

	return SelectedAction;
}

void UDSimSarsaAlgorithmComponent::AddTrainingReward(float Reward)
{
	// Reward is accumulated until the next state-action pair is observed.
	PendingReward += Reward;
}

void UDSimSarsaAlgorithmComponent::SetGoalPosition(const FVector& NewGoalPosition)
{
	GoalPosition = NewGoalPosition;

	ResolveOwnerReferences();

	if (OwnerActor)
	{
		StartPosition = StartPosition.IsNearlyZero()
			                ? OwnerActor->GetActorLocation()
			                : StartPosition;

		PathLength = FMath::Max(10.0f, FVector::Dist(StartPosition, GoalPosition));
	}
}

bool UDSimSarsaAlgorithmComponent::LoadTrainingData(const FString& FileName)
{
	const FString Path = ResolveTrainingDataPath(FileName);

	FString Json;
	if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
	{
		InitializeEmptyTable();
		return false;
	}

	FDSimSarsaTableData LoadedTable;
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

bool UDSimSarsaAlgorithmComponent::SaveTrainingData(const FString& FileName)
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

FString UDSimSarsaAlgorithmComponent::GetAlgorithmName() const
{
	return bUse2DState ? TEXT("SARSA_2D") : TEXT("SARSA_1D");
}

void UDSimSarsaAlgorithmComponent::ResolveOwnerReferences()
{
	if (AIController && OwnerActor)
	{
		return;
	}

	AIController = Cast<ADSimCharacterAIController>(GetOwner());

	if (AIController)
	{
		OwnerActor = Cast<ADSimCharacter>(AIController->GetPawn());
		return;
	}

	OwnerActor = Cast<ADSimCharacter>(GetOwner());

	if (OwnerActor)
	{
		AIController = Cast<ADSimCharacterAIController>(OwnerActor->GetController());
	}
}

void UDSimSarsaAlgorithmComponent::InitializeEmptyTable()
{
	QTable.NumSections = FMath::Max(1, NumSections);
	QTable.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	QTable.LaneHalfWidth = FMath::Max(1.0f, LaneHalfWidth);
	QTable.AllStates.Empty();

	StateIndexByPackedKey.Empty();
}

void UDSimSarsaAlgorithmComponent::RebuildStateIndex()
{
	StateIndexByPackedKey.Empty();

	for (int32 i = 0; i < QTable.AllStates.Num(); ++i)
	{
		StateIndexByPackedKey.Add(QTable.AllStates[i].Key.PackedKey, i);
	}
}

FDSimSarsaStateKey UDSimSarsaAlgorithmComponent::DiscretizeCurrentState() const
{
	FDSimSarsaStateKey Key;

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

FDSimSarsaStateData* UDSimSarsaAlgorithmComponent::FindStateByPackedKey(int32 PackedKey)
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

FDSimSarsaStateData* UDSimSarsaAlgorithmComponent::FindOrAddState(const FDSimSarsaStateKey& Key)
{
	if (FDSimSarsaStateData* ExistingState = FindStateByPackedKey(Key.PackedKey))
	{
		return ExistingState;
	}

	FDSimSarsaStateData NewState;
	NewState.Key = Key;

	const EBotAction DefaultActions[] =
	{
		EBotAction::TowardGoal,
		EBotAction::TowardCover,
		EBotAction::RandomMove
	};

	for (const EBotAction Action : DefaultActions)
	{
		FDSimSarsaActionValue ActionValue;
		ActionValue.BotAction = Action;
		ActionValue.QValue = InitialQValue;
		NewState.Actions.Add(ActionValue);
	}

	const int32 NewIndex = QTable.AllStates.Add(NewState);
	StateIndexByPackedKey.Add(Key.PackedKey, NewIndex);

	return &QTable.AllStates[NewIndex];
}

FDSimSarsaActionValue* UDSimSarsaAlgorithmComponent::FindActionValue(
	FDSimSarsaStateData& State,
	EBotAction Action
)
{
	FDSimSarsaActionValue* ActionValue = State.FindAction(Action);

	if (ActionValue)
	{
		return ActionValue;
	}

	FDSimSarsaActionValue NewActionValue;
	NewActionValue.BotAction = Action;
	NewActionValue.QValue = InitialQValue;

	const int32 NewIndex = State.Actions.Add(NewActionValue);
	return State.Actions.IsValidIndex(NewIndex) ? &State.Actions[NewIndex] : nullptr;
}

const FDSimSarsaActionValue* UDSimSarsaAlgorithmComponent::FindActionValue(
	const FDSimSarsaStateData& State,
	EBotAction Action
) const
{
	return State.FindAction(Action);
}

EBotAction UDSimSarsaAlgorithmComponent::SelectActionEpsilonGreedy(
	const FDSimSarsaStateData& State
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

	for (const FDSimSarsaActionValue& ActionValue : State.Actions)
	{
		if (ActionValue.QValue > BestQ)
		{
			BestQ = ActionValue.QValue;
			BestAction = ActionValue.BotAction;
		}
	}

	return BestAction;
}

float UDSimSarsaAlgorithmComponent::GetQValueForAction(
	const FDSimSarsaStateData& State,
	EBotAction Action
) const
{
	const FDSimSarsaActionValue* ActionValue = FindActionValue(State, Action);
	return ActionValue ? ActionValue->QValue : 0.0f;
}

void UDSimSarsaAlgorithmComponent::ApplySarsaUpdate(
	int32 StatePackedKey,
	EBotAction Action,
	float Reward,
	const FDSimSarsaStateData* NextState,
	EBotAction NextAction,
	bool bTerminal
)
{
	FDSimSarsaStateData* State = FindStateByPackedKey(StatePackedKey);

	if (!State)
	{
		return;
	}

	FDSimSarsaActionValue* ActionValue = FindActionValue(*State, Action);

	if (!ActionValue)
	{
		return;
	}

	const float OldQ = ActionValue->QValue;
	const float NextQ = (!bTerminal && NextState) ? GetQValueForAction(*NextState, NextAction) : 0.0f;

	const float TDTarget = Reward + Gamma * NextQ;
	const float TDError = TDTarget - OldQ;

	float NewQ = OldQ + Alpha * TDError;

	if (SarsaConfig && SarsaConfig->bClampQValues)
	{
		NewQ = FMath::Clamp(NewQ, SarsaConfig->MinQValue, SarsaConfig->MaxQValue);
	}

	ActionValue->QValue = NewQ;
}

void UDSimSarsaAlgorithmComponent::FlushPendingTransitionAsTerminal(float TerminalReward)
{
	if (!bHasLastTransition)
	{
		PendingReward = 0.0f;
		return;
	}

	const float FinalReward = PendingReward + TerminalReward;

	ApplySarsaUpdate(
		LastStatePackedKey,
		LastAction,
		FinalReward,
		nullptr,
		EBotAction::None,
		true
	);

	bHasLastTransition = false;
	LastStatePackedKey = INDEX_NONE;
	LastAction = EBotAction::None;
	PendingReward = 0.0f;
}

void UDSimSarsaAlgorithmComponent::DecayEpsilon()
{
	Epsilon = FMath::Max(EpsilonMin, Epsilon * EpsilonDecay);
}

float UDSimSarsaAlgorithmComponent::GetTerminalRewardForFinishReason(
	EDSimEpisodeFinishReason FinishReason
) const
{
	if (!SarsaConfig)
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
		return SarsaConfig->GoalReachedTerminalReward;

	case EDSimEpisodeFinishReason::BotKilledByDrone:
		return SarsaConfig->BotKilledTerminalReward;

	case EDSimEpisodeFinishReason::DroneCrashed:
		return SarsaConfig->DroneCrashedTerminalReward;

	case EDSimEpisodeFinishReason::Timeout:
		return SarsaConfig->TimeoutTerminalReward;

	case EDSimEpisodeFinishReason::StoppedManually:
	case EDSimEpisodeFinishReason::Unknown:
	default:
		return 0.0f;
	}
}

FString UDSimSarsaAlgorithmComponent::ResolveTrainingDataPath(const FString& FileName) const
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
