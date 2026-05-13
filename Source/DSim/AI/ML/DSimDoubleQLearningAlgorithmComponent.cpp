#include "DSim/AI/ML/DSimDoubleQLearningAlgorithmComponent.h"

#include "DSim/AI/ML/DSimDoubleQLearningAlgorithmConfig.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UDSimDoubleQLearningAlgorithmComponent::UDSimDoubleQLearningAlgorithmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimDoubleQLearningAlgorithmComponent::InitializeAlgorithm(
	const FDSimAlgorithmRuntimeContext& InContext,
	UDSimBotTrainingAlgorithmConfig* InConfig
)
{
	// Avoid calling the base load flow before this component applies its own config.
	RuntimeContext = InContext;
	AlgorithmConfig = InConfig;

	DoubleQConfig = Cast<UDSimDoubleQLearningAlgorithmConfig>(InConfig);

	if (DoubleQConfig)
	{
		Alpha = DoubleQConfig->LearningRate;
		Gamma = DoubleQConfig->DiscountFactor;
		Epsilon = DoubleQConfig->EpsilonStart;
		EpsilonMin = DoubleQConfig->EpsilonMin;
		EpsilonDecay = DoubleQConfig->EpsilonDecay;

		NumSections = FMath::Max(1, DoubleQConfig->HorizontalSections);
		NumLanes = FMath::Max(1, DoubleQConfig->VerticalLanes);
		LaneHalfWidth = FMath::Max(1.0f, DoubleQConfig->LaneHalfWidth);

		InitialQValue = DoubleQConfig->InitialQValue;
		UpdateFirstTableProbability = FMath::Clamp(DoubleQConfig->UpdateFirstTableProbability, 0.0f, 1.0f);
		DefaultSaveFileName = DoubleQConfig->SaveFileName;
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
		InitializeEmptyTables();
	}

	bCurrentEpisodeFinalized = false;
	bHasLastTransition = false;
	PendingReward = 0.0f;
}

void UDSimDoubleQLearningAlgorithmComponent::StartEpisode(int32 EpisodeId)
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

void UDSimDoubleQLearningAlgorithmComponent::EndEpisode(EDSimEpisodeFinishReason FinishReason)
{
	Super::EndEpisode(FinishReason);

	if (bCurrentEpisodeFinalized)
	{
		return;
	}

	const float TerminalReward = GetTerminalRewardForFinishReason(FinishReason);
	FlushPendingTransitionAsTerminal(TerminalReward);

	DecayEpsilon();

	if (DoubleQConfig && DoubleQConfig->bAutoSaveAfterEpisode)
	{
		SaveTrainingData(TEXT(""));
	}

	bCurrentEpisodeFinalized = true;
	RecordTrainingReward(TerminalReward);
}

EBotAction UDSimDoubleQLearningAlgorithmComponent::RequestTrainingAction()
{
	ResolveOwnerReferences();

	if (!OwnerActor)
	{
		return EBotAction::None;
	}

	const FDSimDoubleQStateKey CurrentKey = DiscretizeCurrentState();
	EnsureStateInBothTables(CurrentKey);

	FDSimDoubleQStateData* CurrentStateA = FindStateByPackedKey(
		QData.QTableA,
		StateIndexByPackedKeyA,
		CurrentKey.PackedKey
	);

	FDSimDoubleQStateData* CurrentStateB = FindStateByPackedKey(
		QData.QTableB,
		StateIndexByPackedKeyB,
		CurrentKey.PackedKey
	);

	if (!CurrentStateA || !CurrentStateB)
	{
		return EBotAction::None;
	}

	// Update the previous transition when the next state is known.
	if (bHasLastTransition)
	{
		ApplyDoubleQUpdate(
			LastStatePackedKey,
			LastAction,
			PendingReward,
			CurrentKey.PackedKey,
			false
		);

		PendingReward = 0.0f;
	}

	const EBotAction SelectedAction = SelectActionEpsilonGreedy(*CurrentStateA, *CurrentStateB);

	LastStatePackedKey = CurrentKey.PackedKey;
	LastAction = SelectedAction;
	bHasLastTransition = true;

	return SelectedAction;
}

void UDSimDoubleQLearningAlgorithmComponent::AddTrainingReward(float Reward)
{
	// Reward is accumulated until the next state is observed.
	PendingReward += Reward;
}

void UDSimDoubleQLearningAlgorithmComponent::SetGoalPosition(const FVector& NewGoalPosition)
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

bool UDSimDoubleQLearningAlgorithmComponent::LoadTrainingData(const FString& FileName)
{
	const FString Path = ResolveTrainingDataPath(FileName);

	FString Json;
	if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
	{
		InitializeEmptyTables();
		return false;
	}

	FDSimDoubleQTableData LoadedData;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &LoadedData, 0, 0))
	{
		InitializeEmptyTables();
		return false;
	}

	QData = LoadedData;

	NumSections = FMath::Max(1, QData.NumSections);
	NumLanes = FMath::Max(1, QData.NumLanes);
	LaneHalfWidth = FMath::Max(1.0f, QData.LaneHalfWidth);

	if (!bUse2DState)
	{
		NumLanes = 1;
		QData.NumLanes = 1;
	}

	RebuildStateIndexes();

	return true;
}

bool UDSimDoubleQLearningAlgorithmComponent::SaveTrainingData(const FString& FileName)
{
	QData.NumSections = FMath::Max(1, NumSections);
	QData.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	QData.LaneHalfWidth = FMath::Max(1.0f, LaneHalfWidth);

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(QData, Json))
	{
		return false;
	}

	const FString Path = ResolveTrainingDataPath(FileName);
	const FString Directory = FPaths::GetPath(Path);

	IFileManager::Get().MakeDirectory(*Directory, true);

	return FFileHelper::SaveStringToFile(Json, *Path);
}

FString UDSimDoubleQLearningAlgorithmComponent::GetAlgorithmName() const
{
	return bUse2DState ? TEXT("Double_Q_Learning_2D") : TEXT("Double_Q_Learning_1D");
}

void UDSimDoubleQLearningAlgorithmComponent::ResolveOwnerReferences()
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

void UDSimDoubleQLearningAlgorithmComponent::InitializeEmptyTables()
{
	QData.NumSections = FMath::Max(1, NumSections);
	QData.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	QData.LaneHalfWidth = FMath::Max(1.0f, LaneHalfWidth);
	QData.QTableA.Empty();
	QData.QTableB.Empty();

	StateIndexByPackedKeyA.Empty();
	StateIndexByPackedKeyB.Empty();
}

void UDSimDoubleQLearningAlgorithmComponent::RebuildStateIndexes()
{
	StateIndexByPackedKeyA.Empty();
	StateIndexByPackedKeyB.Empty();

	for (int32 i = 0; i < QData.QTableA.Num(); ++i)
	{
		StateIndexByPackedKeyA.Add(QData.QTableA[i].Key.PackedKey, i);
	}

	for (int32 i = 0; i < QData.QTableB.Num(); ++i)
	{
		StateIndexByPackedKeyB.Add(QData.QTableB[i].Key.PackedKey, i);
	}
}

FDSimDoubleQStateKey UDSimDoubleQLearningAlgorithmComponent::DiscretizeCurrentState() const
{
	FDSimDoubleQStateKey Key;

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

FDSimDoubleQStateData* UDSimDoubleQLearningAlgorithmComponent::FindStateByPackedKey(
	TArray<FDSimDoubleQStateData>& Table,
	TMap<int32, int32>& IndexMap,
	int32 PackedKey
)
{
	const int32* FoundIndex = IndexMap.Find(PackedKey);

	if (!FoundIndex)
	{
		return nullptr;
	}

	return Table.IsValidIndex(*FoundIndex) ? &Table[*FoundIndex] : nullptr;
}

const FDSimDoubleQStateData* UDSimDoubleQLearningAlgorithmComponent::FindStateByPackedKey(
	const TArray<FDSimDoubleQStateData>& Table,
	const TMap<int32, int32>& IndexMap,
	int32 PackedKey
) const
{
	const int32* FoundIndex = IndexMap.Find(PackedKey);

	if (!FoundIndex)
	{
		return nullptr;
	}

	return Table.IsValidIndex(*FoundIndex) ? &Table[*FoundIndex] : nullptr;
}

FDSimDoubleQStateData* UDSimDoubleQLearningAlgorithmComponent::FindOrAddState(
	TArray<FDSimDoubleQStateData>& Table,
	TMap<int32, int32>& IndexMap,
	const FDSimDoubleQStateKey& Key
)
{
	if (FDSimDoubleQStateData* ExistingState = FindStateByPackedKey(Table, IndexMap, Key.PackedKey))
	{
		return ExistingState;
	}

	FDSimDoubleQStateData NewState;
	NewState.Key = Key;

	const EBotAction DefaultActions[] =
	{
		EBotAction::TowardGoal,
		EBotAction::TowardCover,
		EBotAction::RandomMove
	};

	for (const EBotAction Action : DefaultActions)
	{
		FDSimDoubleQActionValue ActionValue;
		ActionValue.BotAction = Action;
		ActionValue.QValue = InitialQValue;
		NewState.Actions.Add(ActionValue);
	}

	const int32 NewIndex = Table.Add(NewState);
	IndexMap.Add(Key.PackedKey, NewIndex);

	return &Table[NewIndex];
}

void UDSimDoubleQLearningAlgorithmComponent::EnsureStateInBothTables(
	const FDSimDoubleQStateKey& Key
)
{
	FindOrAddState(QData.QTableA, StateIndexByPackedKeyA, Key);
	FindOrAddState(QData.QTableB, StateIndexByPackedKeyB, Key);
}

FDSimDoubleQActionValue* UDSimDoubleQLearningAlgorithmComponent::FindActionValue(
	FDSimDoubleQStateData& State,
	EBotAction Action
)
{
	FDSimDoubleQActionValue* ActionValue = State.FindAction(Action);

	if (ActionValue)
	{
		return ActionValue;
	}

	FDSimDoubleQActionValue NewActionValue;
	NewActionValue.BotAction = Action;
	NewActionValue.QValue = InitialQValue;

	const int32 NewIndex = State.Actions.Add(NewActionValue);
	return State.Actions.IsValidIndex(NewIndex) ? &State.Actions[NewIndex] : nullptr;
}

const FDSimDoubleQActionValue* UDSimDoubleQLearningAlgorithmComponent::FindActionValue(
	const FDSimDoubleQStateData& State,
	EBotAction Action
) const
{
	return State.FindAction(Action);
}

EBotAction UDSimDoubleQLearningAlgorithmComponent::SelectActionEpsilonGreedy(
	const FDSimDoubleQStateData& StateA,
	const FDSimDoubleQStateData& StateB
) const
{
	if (StateA.Actions.Num() == 0)
	{
		return EBotAction::None;
	}

	if (FMath::FRand() < Epsilon)
	{
		const int32 RandomIndex = FMath::RandRange(0, StateA.Actions.Num() - 1);
		return StateA.Actions[RandomIndex].BotAction;
	}

	float BestQ = -FLT_MAX;
	EBotAction BestAction = EBotAction::None;

	for (const FDSimDoubleQActionValue& ActionValue : StateA.Actions)
	{
		const float CombinedQ = GetCombinedQValue(StateA, StateB, ActionValue.BotAction);

		if (CombinedQ > BestQ)
		{
			BestQ = CombinedQ;
			BestAction = ActionValue.BotAction;
		}
	}

	return BestAction;
}

EBotAction UDSimDoubleQLearningAlgorithmComponent::GetBestActionFromTable(
	const FDSimDoubleQStateData& State
) const
{
	float BestQ = -FLT_MAX;
	EBotAction BestAction = EBotAction::None;

	for (const FDSimDoubleQActionValue& ActionValue : State.Actions)
	{
		if (ActionValue.QValue > BestQ)
		{
			BestQ = ActionValue.QValue;
			BestAction = ActionValue.BotAction;
		}
	}

	return BestAction;
}

float UDSimDoubleQLearningAlgorithmComponent::GetQValueForAction(
	const FDSimDoubleQStateData& State,
	EBotAction Action
) const
{
	const FDSimDoubleQActionValue* ActionValue = FindActionValue(State, Action);
	return ActionValue ? ActionValue->QValue : 0.0f;
}

float UDSimDoubleQLearningAlgorithmComponent::GetCombinedQValue(
	const FDSimDoubleQStateData& StateA,
	const FDSimDoubleQStateData& StateB,
	EBotAction Action
) const
{
	return GetQValueForAction(StateA, Action) + GetQValueForAction(StateB, Action);
}

void UDSimDoubleQLearningAlgorithmComponent::ApplyDoubleQUpdate(
	int32 StatePackedKey,
	EBotAction Action,
	float Reward,
	int32 NextStatePackedKey,
	bool bTerminal
)
{
	FDSimDoubleQStateData* StateA = FindStateByPackedKey(
		QData.QTableA,
		StateIndexByPackedKeyA,
		StatePackedKey
	);

	FDSimDoubleQStateData* StateB = FindStateByPackedKey(
		QData.QTableB,
		StateIndexByPackedKeyB,
		StatePackedKey
	);

	if (!StateA || !StateB)
	{
		return;
	}

	const bool bUpdateA = FMath::FRand() < UpdateFirstTableProbability;

	TArray<FDSimDoubleQStateData>& UpdateTable = bUpdateA ? QData.QTableA : QData.QTableB;
	TArray<FDSimDoubleQStateData>& EvalTable = bUpdateA ? QData.QTableB : QData.QTableA;

	TMap<int32, int32>& UpdateIndex = bUpdateA ? StateIndexByPackedKeyA : StateIndexByPackedKeyB;
	TMap<int32, int32>& EvalIndex = bUpdateA ? StateIndexByPackedKeyB : StateIndexByPackedKeyA;

	FDSimDoubleQStateData* UpdateState = FindStateByPackedKey(UpdateTable, UpdateIndex, StatePackedKey);

	if (!UpdateState)
	{
		return;
	}

	FDSimDoubleQActionValue* UpdateActionValue = FindActionValue(*UpdateState, Action);

	if (!UpdateActionValue)
	{
		return;
	}

	float NextEstimate = 0.0f;

	if (!bTerminal)
	{
		FDSimDoubleQStateData* NextUpdateState = FindStateByPackedKey(UpdateTable, UpdateIndex, NextStatePackedKey);
		FDSimDoubleQStateData* NextEvalState = FindStateByPackedKey(EvalTable, EvalIndex, NextStatePackedKey);

		if (NextUpdateState && NextEvalState)
		{
			const EBotAction BestNextAction = GetBestActionFromTable(*NextUpdateState);
			NextEstimate = GetQValueForAction(*NextEvalState, BestNextAction);
		}
	}

	const float OldQ = UpdateActionValue->QValue;
	const float TDTarget = Reward + Gamma * NextEstimate;
	const float TDError = TDTarget - OldQ;

	float NewQ = OldQ + Alpha * TDError;

	if (DoubleQConfig && DoubleQConfig->bClampQValues)
	{
		NewQ = FMath::Clamp(NewQ, DoubleQConfig->MinQValue, DoubleQConfig->MaxQValue);
	}

	UpdateActionValue->QValue = NewQ;
}

void UDSimDoubleQLearningAlgorithmComponent::FlushPendingTransitionAsTerminal(
	float TerminalReward
)
{
	if (!bHasLastTransition)
	{
		PendingReward = 0.0f;
		return;
	}

	const float FinalReward = PendingReward + TerminalReward;

	ApplyDoubleQUpdate(
		LastStatePackedKey,
		LastAction,
		FinalReward,
		INDEX_NONE,
		true
	);

	bHasLastTransition = false;
	LastStatePackedKey = INDEX_NONE;
	LastAction = EBotAction::None;
	PendingReward = 0.0f;
}

void UDSimDoubleQLearningAlgorithmComponent::DecayEpsilon()
{
	Epsilon = FMath::Max(EpsilonMin, Epsilon * EpsilonDecay);
}

float UDSimDoubleQLearningAlgorithmComponent::GetTerminalRewardForFinishReason(
	EDSimEpisodeFinishReason FinishReason
) const
{
	if (!DoubleQConfig)
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
		return DoubleQConfig->GoalReachedTerminalReward;

	case EDSimEpisodeFinishReason::BotKilledByDrone:
		return DoubleQConfig->BotKilledTerminalReward;

	case EDSimEpisodeFinishReason::DroneCrashed:
		return DoubleQConfig->DroneCrashedTerminalReward;

	case EDSimEpisodeFinishReason::Timeout:
		return DoubleQConfig->TimeoutTerminalReward;

	case EDSimEpisodeFinishReason::StoppedManually:
	case EDSimEpisodeFinishReason::Unknown:
	default:
		return 0.0f;
	}
}

FString UDSimDoubleQLearningAlgorithmComponent::ResolveTrainingDataPath(
	const FString& FileName
) const
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
