#include "DSimReinforcementLearningComp.h"

#include "DrawDebugHelpers.h"
#include "JsonObjectConverter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/DSimDebugComponent.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ============================================================
// Strategy implementations
// ============================================================

class FRLDiscretizer1D final : public IRLStateDiscretizer
{
public:
	virtual FDSimRLStateKey Discretize(
		const FVector& Pos,
		const FVector& Start,
		const FVector& Goal,
		float InPathLength,
		int32 InNumSections,
		int32 InNumLanes,
		float /*LaneHalfWidth*/) const override
	{
		FDSimRLStateKey Key;

		const int32 SafeSections = FMath::Max(1, InNumSections);
		const float SafePath = FMath::Max(10.f, InPathLength);

		const float Progress = 1.f - FVector::Dist(Pos, Goal) / SafePath;
		Key.SectionIndex = FMath::Clamp(FMath::RoundToInt(Progress * float(SafeSections)), 0, SafeSections);
		Key.SectionKey = float(Key.SectionIndex) / float(SafeSections);
		Key.LaneIndex = 0;
		Key.RebuildPackedKey(1);
		return Key;
	}
};

class FRLDiscretizer2D final : public IRLStateDiscretizer
{
public:
	virtual FDSimRLStateKey Discretize(
		const FVector& Pos,
		const FVector& Start,
		const FVector& Goal,
		float InPathLength,
		int32 InNumSections,
		int32 InNumLanes,
		float InLaneHalfWidth) const override
	{
		FDSimRLStateKey Key;

		const int32 SafeSections = FMath::Max(1, InNumSections);
		const int32 SafeLanes = FMath::Max(1, InNumLanes);
		const float SafePath = FMath::Max(10.f, InPathLength);
		const float HalfWidth = FMath::Max(1.f, InLaneHalfWidth);

		const float Progress = 1.f - FVector::Dist(Pos, Goal) / SafePath;
		Key.SectionIndex = FMath::Clamp(FMath::RoundToInt(Progress * float(SafeSections)), 0, SafeSections);
		Key.SectionKey = float(Key.SectionIndex) / float(SafeSections);

		const FVector Dir = (Goal - Start).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const float Offset = FVector::DotProduct((Pos - Start), Right);

		const float LaneWidth = (2.f * HalfWidth) / float(SafeLanes);
		const float Normalized = (Offset + HalfWidth) / FMath::Max(1.f, LaneWidth);
		Key.LaneIndex = FMath::Clamp(FMath::FloorToInt(Normalized), 0, SafeLanes - 1);

		Key.RebuildPackedKey(SafeLanes);
		return Key;
	}
};

class FEpsilonGreedyPolicy final : public IRLActionPolicy
{
public:
	virtual EBotAction SelectAction(const TArray<FDSimBotActionData>& Actions, float InEpsilon) const override
	{
		if (Actions.Num() == 0)
		{
			return EBotAction::None;
		}

		if (FMath::FRand() < InEpsilon)
		{
			return static_cast<EBotAction>(FMath::RandRange(1, 3));
		}

		float MaxQ = -FLT_MAX;
		EBotAction Best = EBotAction::None;

		for (const FDSimBotActionData& A : Actions)
		{
			if (A.QValue > MaxQ)
			{
				MaxQ = A.QValue;
				Best = A.BotAction;
			}
		}

		return Best;
	}
};

class FRLJsonRepository final : public IRLDataRepository
{
public:
	virtual bool Save(const FDSimRLData2D& Data, const FString& Path) override
	{
		FString Json;
		if (!FJsonObjectConverter::UStructToJsonObjectString(Data, Json))
		{
			return false;
		}
		return FFileHelper::SaveStringToFile(Json, *Path);
	}

	virtual bool Load(FDSimRLData2D& OutData, const FString& Path) override
	{
		FString Json;
		if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
		{
			return false;
		}
		return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutData, 0, 0);
	}

	virtual bool LoadLegacy1D(FDSimRLData& OutLegacy, const FString& Path) override
	{
		FString Json;
		if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Json, *Path))
		{
			return false;
		}
		return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutLegacy, 0, 0);
	}
};

// ============================================================
// Component
// ============================================================

UDSimReinforcementLearningComp::UDSimReinforcementLearningComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void UDSimReinforcementLearningComp::InitComponentData()
{
	AIController = Cast<ADSimCharacterAIController>(GetOwner());
	if (!AIController)
	{
		return;
	}

	OwnerActor = Cast<ADSimCharacter>(AIController->GetPawn());
	if (!OwnerActor)
	{
		return;
	}

	StartPosition = OwnerActor->GetActorLocation();

	if (GoalPosition.IsNearlyZero())
	{
		TArray<AActor*> Goals;
		UGameplayStatics::GetAllActorsOfClass(this, ADSimGoalActor::StaticClass(), Goals);
		if (!Goals.IsEmpty())
		{
			GoalPosition = Goals[0]->GetActorLocation();
		}
	}

	PathLength = FMath::Max(10.f, FVector::Dist(StartPosition, GoalPosition));

	StateDiscretizer = bUse2DState ? TUniquePtr<IRLStateDiscretizer>(new FRLDiscretizer2D())
		: TUniquePtr<IRLStateDiscretizer>(new FRLDiscretizer1D());

	ActionPolicy = TUniquePtr<IRLActionPolicy>(new FEpsilonGreedyPolicy());
	DataRepository = TUniquePtr<IRLDataRepository>(new FRLJsonRepository());

	LoadRLDataFromFile();
	StartNewEpisode();
}

void UDSimReinforcementLearningComp::SetGoalPosition(const FVector& NewGoalPosition)
{
	GoalPosition = NewGoalPosition;
	if (OwnerActor)
	{
		StartPosition = StartPosition.IsNearlyZero() ? OwnerActor->GetActorLocation() : StartPosition;
		PathLength = FMath::Max(10.f, FVector::Dist(StartPosition, GoalPosition));
	}
}

void UDSimReinforcementLearningComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawDebug || !GetWorld() || !OwnerActor)
	{
		return;
	}

	if (!CurrentMoveTarget.IsNearlyZero())
	{
		DrawDebugSphere(GetWorld(), CurrentMoveTarget, 50.f, 16, FColor::Red, false, 0.f, 0, 2.f);
	}

	if (NumSections <= 0 || PathLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Dir = (GoalPosition - StartPosition).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const float HalfWidth = bUse2DState ? FMath::Max(1.f, LaneHalfWidth) : 30000.f;
	const int32 SafeSections = FMath::Max(1, NumSections);

	for (int32 i = 0; i <= SafeSections; ++i)
	{
		const float T = float(i) / float(SafeSections);
		const FVector Center = StartPosition + Dir * (PathLength * T);
		const FVector A = Center - Right * HalfWidth;
		const FVector B = Center + Right * HalfWidth;

		const int32 CurrentIndex = DebugData.CurrentSectionIndex;
		const FColor Color = (i == CurrentIndex) ? FColor::Yellow : FColor::Green;

		DrawDebugLine(GetWorld(), A, B, Color, false, 0.f, 0, 8.f);

		if (i % 5 == 0)
		{
			DrawDebugString(
				GetWorld(),
				Center + FVector(0.f, 0.f, 80.f),
				FString::Printf(TEXT("%d"), i),
				nullptr,
				FColor::White,
				0.f,
				false);
		}
	}

	if (bUse2DState)
	{
		const int32 SafeLanes = FMath::Max(1, NumLanes);
		if (SafeLanes > 1)
		{
			const float LaneWidth = (2.f * HalfWidth) / float(SafeLanes);
			const int32 CurrentLane = FMath::Clamp(DebugData.CurrentLaneIndex, 0, SafeLanes - 1);

			for (int32 b = 0; b <= SafeLanes; ++b)
			{
				const float Offset = -HalfWidth + float(b) * LaneWidth;
				const FVector A = StartPosition + Right * Offset;
				const FVector B = GoalPosition + Right * Offset;

				FColor C = FColor(80, 160, 255);
				if (b == CurrentLane || b == (CurrentLane + 1))
				{
					C = FColor::Yellow;
				}
				DrawDebugLine(GetWorld(), A, B, C, false, 0.f, 0, 9.5f);
			}

			for (int32 l = 0; l < SafeLanes; ++l)
			{
				const float CenterOffset = -HalfWidth + (float(l) + 0.5f) * LaneWidth;
				const FVector LabelPos = StartPosition + Right * CenterOffset + FVector(0.f, 0.f, 120.f);
				DrawDebugString(
					GetWorld(),
					LabelPos,
					FString::Printf(TEXT("L%d"), l),
					nullptr,
					FColor::White,
					0.f,
					false);
			}
		}
	}
}

void UDSimReinforcementLearningComp::SetCurrentMoveTarget(const FVector& NewTarget)
{
	CurrentMoveTarget = NewTarget;
}

void UDSimReinforcementLearningComp::SetDrawDebug(bool bEnable)
{
	bDrawDebug = bEnable;
}

FDSimRLStateData2D* UDSimReinforcementLearningComp::FindStateByPacked(int32 PackedKey)
{
	if (const int32* Idx = StateIndexByPackedKey.Find(PackedKey))
	{
		return RLData2D.AllStates.IsValidIndex(*Idx) ? &RLData2D.AllStates[*Idx] : nullptr;
	}
	return nullptr;
}

FDSimRLStateData2D* UDSimReinforcementLearningComp::FindOrAddState(const FDSimRLStateKey& Key)
{
	if (FDSimRLStateData2D* Found = FindStateByPacked(Key.PackedKey))
	{
		return Found;
	}

	FDSimRLStateData2D NewState;
	NewState.Key = Key;

	for (uint8 i = 1; i <= 3; ++i)
	{
		FDSimBotActionData A;
		A.BotAction = static_cast<EBotAction>(i);
		A.QValue = 0.f;
		NewState.Actions.Add(A);
	}

	const int32 NewIndex = RLData2D.AllStates.Add(NewState);
	StateIndexByPackedKey.Add(Key.PackedKey, NewIndex);
	return &RLData2D.AllStates[NewIndex];
}

EBotAction UDSimReinforcementLearningComp::RequestAction()
{
	if (!OwnerActor || PathLength <= KINDA_SMALL_NUMBER)
	{
		return EBotAction::None;
	}

	const int32 EffectiveLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	const float EffectiveHalfWidth = bUse2DState ? FMath::Max(1.f, LaneHalfWidth) : 0.f;

	if (!StateDiscretizer)
	{
		StateDiscretizer = bUse2DState ? TUniquePtr<IRLStateDiscretizer>(new FRLDiscretizer2D())
			: TUniquePtr<IRLStateDiscretizer>(new FRLDiscretizer1D());
	}
	if (!ActionPolicy)
	{
		ActionPolicy = TUniquePtr<IRLActionPolicy>(new FEpsilonGreedyPolicy());
	}

	const FDSimRLStateKey Key = StateDiscretizer->Discretize(
		OwnerActor->GetActorLocation(),
		StartPosition,
		GoalPosition,
		PathLength,
		NumSections,
		EffectiveLanes,
		EffectiveHalfWidth);

	const bool bStateChanged = (bHasLastEnteredSection
		&& (Key.SectionIndex != DebugData.CurrentSectionIndex || Key.LaneIndex != DebugData.CurrentLaneIndex));

	if (bStateChanged)
	{
		DebugData.PreviousSections.Insert(LastEnteredSection, 0);
		if (DebugData.PreviousSections.Num() > 5)
		{
			DebugData.PreviousSections.SetNum(5);
		}
	}

	FDSimRLStateData2D* State = FindOrAddState(Key);

	const EBotAction Action = ActionPolicy->SelectAction(State->Actions, Epsilon);

	FEpisodeStep Step;
	Step.PackedKey = Key.PackedKey;
	Step.SectionIndex = Key.SectionIndex;
	Step.LaneIndex = Key.LaneIndex;
	Step.SectionKey = Key.SectionKey;
	Step.Action = Action;
	Step.Reward = 0.f;
	CurrentEpisode.Add(Step);

	DebugData.CurrentSectionKey = Key.SectionKey;
	DebugData.CurrentSectionIndex = Key.SectionIndex;
	DebugData.CurrentLaneIndex = Key.LaneIndex;

	float QGoal = 0.f;
	float QCover = 0.f;
	float QRandom = 0.f;
	ExtractQValues(State->Actions, QGoal, QCover, QRandom);

	LastEnteredSection = FRLSectionStepDebug();
	LastEnteredSection.SectionKey = Key.SectionKey;
	LastEnteredSection.SectionIndex = Key.SectionIndex;
	LastEnteredSection.LaneIndex = Key.LaneIndex;
	LastEnteredSection.PackedKey = Key.PackedKey;

	LastEnteredSection.InitialQTowardGoal = QGoal;
	LastEnteredSection.InitialQTowardCover = QCover;
	LastEnteredSection.InitialQRandomMove = QRandom;

	LastEnteredSection.QTowardGoal = QGoal;
	LastEnteredSection.QTowardCover = QCover;
	LastEnteredSection.QRandomMove = QRandom;

	bHasLastEnteredSection = true;

	UpdateDebugNextSections(Key);

	UDSimDebugComponent* DebugComponent = UDSimBlueprintFunctionLibrary::GetDebugComponent(GetWorld());
	if (IsValid(DebugComponent))
	{
		DebugComponent->UpdateSelectedAction(Action);
		DebugComponent->UpdateScores(QGoal, QCover, QRandom);
	}

	return Action;
}

void UDSimReinforcementLearningComp::ApplyReward(float Reward, bool bEpisodeEnd)
{
	if (CurrentEpisode.Num() > 0)
	{
		CurrentEpisode.Last().Reward = Reward;
	}

	UDSimDebugComponent* DebugComponent = UDSimBlueprintFunctionLibrary::GetDebugComponent(GetWorld());
	if (IsValid(DebugComponent))
	{
		DebugComponent->UpdateReward(Reward);
	}

	if (bEpisodeEnd)
	{
		OnEpisodeEnd();
		StartNewEpisode();
		DecayEpsilon();
	}
}

void UDSimReinforcementLearningComp::ExtractQValues(
	const TArray<FDSimBotActionData>& Actions,
	float& OutGoal,
	float& OutCover,
	float& OutRandom) const
{
	OutGoal = 0.f;
	OutCover = 0.f;
	OutRandom = 0.f;

	for (const FDSimBotActionData& A : Actions)
	{
		switch (A.BotAction)
		{
		case EBotAction::TowardGoal:
			OutGoal = A.QValue;
			break;
		case EBotAction::TowardCover:
			OutCover = A.QValue;
			break;
		case EBotAction::RandomMove:
			OutRandom = A.QValue;
			break;
		default:
			break;
		}
	}
}

void UDSimReinforcementLearningComp::UpdateDebugNextSections(const FDSimRLStateKey& CurrentKey)
{
	DebugData.NextSections.Reset();

	const int32 SafeSections = FMath::Max(1, NumSections);
	const int32 SafeLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	const int32 Lane = FMath::Clamp(CurrentKey.LaneIndex, 0, SafeLanes - 1);

	int32 NumAdded = 0;

	for (int32 s = CurrentKey.SectionIndex + 1; s <= SafeSections; ++s)
	{
		FDSimRLStateKey K;
		K.SectionIndex = s;
		K.LaneIndex = Lane;
		K.SectionKey = float(s) / float(SafeSections);
		K.RebuildPackedKey(SafeLanes);

		float QGoal = 0.f;
		float QCover = 0.f;
		float QRandom = 0.f;

		if (FDSimRLStateData2D* State = FindStateByPacked(K.PackedKey))
		{
			ExtractQValues(State->Actions, QGoal, QCover, QRandom);
		}

		FRLSectionStepDebug StepDebug;
		StepDebug.SectionKey = K.SectionKey;
		StepDebug.SectionIndex = s;
		StepDebug.LaneIndex = Lane;
		StepDebug.PackedKey = K.PackedKey;

		StepDebug.QTowardGoal = QGoal;
		StepDebug.QTowardCover = QCover;
		StepDebug.QRandomMove = QRandom;

		StepDebug.InitialQTowardGoal = QGoal;
		StepDebug.InitialQTowardCover = QCover;
		StepDebug.InitialQRandomMove = QRandom;

		DebugData.NextSections.Add(StepDebug);

		if (++NumAdded >= 5)
		{
			break;
		}
	}

	OnDebugDataUpdated.Broadcast(DebugData);
}

void UDSimReinforcementLearningComp::OnEpisodeEnd()
{
	if (CurrentEpisode.Num() == 0)
	{
		return;
	}

	float CumulativeReward = 0.f;

	for (int32 i = CurrentEpisode.Num() - 1; i >= 0; --i)
	{
		const int32 PackedKey = CurrentEpisode[i].PackedKey;
		const EBotAction Action = CurrentEpisode[i].Action;

		CumulativeReward = CurrentEpisode[i].Reward + Gamma * CumulativeReward;

		FDSimRLStateData2D* State = FindStateByPacked(PackedKey);
		if (!State)
		{
			continue;
		}

		FDSimBotActionData* ActionData = State->FindAction(Action);
		if (!ActionData)
		{
			continue;
		}

		ActionData->QValue += Alpha * (CumulativeReward - ActionData->QValue);
		ActionData->QValue = FMath::Clamp(ActionData->QValue, -1.f, 1.f);
	}

	ReplayBuffer.Add(CurrentEpisode);
	if (ReplayBuffer.Num() > ReplayBufferSize)
	{
		ReplayBuffer.RemoveAt(0);
	}

	SaveRLDataToFile();

	LogRLDebug(TEXT("Episode finished: reward propagated."));

	FDSimRLStateKey Current;
	Current.SectionIndex = DebugData.CurrentSectionIndex;
	Current.LaneIndex = DebugData.CurrentLaneIndex;
	Current.SectionKey = DebugData.CurrentSectionKey;
	Current.RebuildPackedKey(bUse2DState ? FMath::Max(1, NumLanes) : 1);
	UpdateDebugNextSections(Current);
}

void UDSimReinforcementLearningComp::StartNewEpisode()
{
	CurrentEpisode.Empty();
	bHasLastEnteredSection = false;

	if (OwnerActor)
	{
		StartPosition = OwnerActor->GetActorLocation();
		PathLength = FMath::Max(10.f, FVector::Dist(StartPosition, GoalPosition));
	}
}

void UDSimReinforcementLearningComp::DecayEpsilon()
{
	Epsilon = FMath::Max(Epsilon * EpsilonDecay, EpsilonMin);
}

void UDSimReinforcementLearningComp::LogRLDebug(const FString& Msg)
{
	UE_LOG(LogTemp, Log, TEXT("[RL] %s | Epsilon=%.3f"), *Msg, Epsilon);
}

void UDSimReinforcementLearningComp::RebuildStateIndex()
{
	StateIndexByPackedKey.Reset();
	for (int32 i = 0; i < RLData2D.AllStates.Num(); ++i)
	{
		StateIndexByPackedKey.Add(RLData2D.AllStates[i].Key.PackedKey, i);
	}
}

bool UDSimReinforcementLearningComp::SaveRLDataToFile()
{
	if (!DataRepository)
	{
		DataRepository = TUniquePtr<IRLDataRepository>(new FRLJsonRepository());
	}

	RLData2D.NumSections = FMath::Max(1, NumSections);
	RLData2D.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	RLData2D.LaneHalfWidth = bUse2DState ? FMath::Max(1.f, LaneHalfWidth) : RLData2D.LaneHalfWidth;

	const FString Path = GetRLDataSavePath();
	return DataRepository->Save(RLData2D, Path);
}

bool UDSimReinforcementLearningComp::LoadRLDataFromFile()
{
	if (!DataRepository)
	{
		DataRepository = TUniquePtr<IRLDataRepository>(new FRLJsonRepository());
	}

	RLData2D.AllStates.Empty();
	StateIndexByPackedKey.Empty();

	const FString ModePath = GetRLDataSavePath();
	FDSimRLData2D Loaded2D;

	if (DataRepository->Load(Loaded2D, ModePath))
	{
		RLData2D = Loaded2D;
		NumSections = FMath::Max(1, RLData2D.NumSections);
		NumLanes = FMath::Max(1, RLData2D.NumLanes);
		LaneHalfWidth = FMath::Max(1.f, RLData2D.LaneHalfWidth);
		RebuildStateIndex();
		return true;
	}

	// Якщо режим 2D, а файл 2D не знайдено або формат не той,
	// пробуємо підхопити legacy 1D файл і мігрувати в lane 0.
	if (bUse2DState)
	{
		const FString LegacyPath = (FPaths::ProjectSavedDir() / TEXT("SaveGames/")) / SaveFileName1D;
		FDSimRLData Legacy;

		if (DataRepository->LoadLegacy1D(Legacy, LegacyPath))
		{
			RLData2D.NumSections = FMath::Max(1, NumSections);
			RLData2D.NumLanes = FMath::Max(1, NumLanes);
			RLData2D.LaneHalfWidth = FMath::Max(1.f, LaneHalfWidth);

			const int32 SafeSections = RLData2D.NumSections;
			const int32 SafeLanes = RLData2D.NumLanes;

			for (const FDSimRLSectionData& S : Legacy.AllSections)
			{
				const int32 SectionIndex = FMath::Clamp(FMath::RoundToInt(S.SectionKey * float(SafeSections)), 0, SafeSections);

				FDSimRLStateKey K;
				K.SectionIndex = SectionIndex;
				K.SectionKey = float(SectionIndex) / float(SafeSections);
				K.LaneIndex = 0;
				K.RebuildPackedKey(SafeLanes);

				FDSimRLStateData2D State;
				State.Key = K;
				State.Actions = S.Actions;
				RLData2D.AllStates.Add(State);
			}

			RebuildStateIndex();
			SaveRLDataToFile();
			return true;
		}
	}

	RLData2D.NumSections = FMath::Max(1, NumSections);
	RLData2D.NumLanes = bUse2DState ? FMath::Max(1, NumLanes) : 1;
	RLData2D.LaneHalfWidth = FMath::Max(1.f, LaneHalfWidth);
	RLData2D.AllStates.Empty();
	StateIndexByPackedKey.Empty();
	return false;
}

FString UDSimReinforcementLearningComp::GetRLDataSavePath() const
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames/");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString FileName = bUse2DState ? SaveFileName2D : SaveFileName1D;
	return Dir / FileName;
}
