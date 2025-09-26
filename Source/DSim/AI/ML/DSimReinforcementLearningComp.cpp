#include "DSimReinforcementLearningComp.h"
#include "AIController.h"
#include "JsonObjectConverter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/DSimDebugComponent.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

UDSimReinforcementLearningComp::UDSimReinforcementLearningComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void UDSimReinforcementLearningComp::InitComponentData()
{
	AIController = Cast<ADSimCharacterAIController>(GetOwner());
	if (!AIController) return;
	OwnerActor = Cast<ADSimCharacter>(AIController->GetPawn());
	if (!OwnerActor) return;

	TArray<AActor*> Goals;
	UGameplayStatics::GetAllActorsOfClass(this, ADSimGoalActor::StaticClass(), Goals);
	if (Goals.IsEmpty()) return;

	StartPosition = OwnerActor->GetActorLocation();
	GoalPosition = Goals[0]->GetActorLocation();
	PathLength = FMath::Max(10.f, FVector::Dist(StartPosition, GoalPosition));

	LoadRLDataFromFile();
	StartNewEpisode();
}

void UDSimReinforcementLearningComp::TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*)
{
	// Можна викликати RequestAction/ApplyReward вручну з BT Task
}

EBotAction UDSimReinforcementLearningComp::RequestAction()
{
	// 1. Визначаємо поточну секцію (SectionKey)
	const float Progress = 1.f - FVector::Dist(OwnerActor->GetActorLocation(), GoalPosition) / PathLength;
	const float SectionStep = 1.0f / float(NumSections);
	const float SectionKey = FMath::RoundToFloat(Progress / SectionStep) * SectionStep;

	// 2. Шукаємо секцію або створюємо нову
	FDSimRLSectionData* Section = RLData.FindSection(SectionKey);
	if (!Section)
	{
		FDSimRLSectionData NewSec;
		NewSec.SectionKey = SectionKey;
		for (uint8 i = 1; i <= 3; ++i)
		{
			FDSimBotActionData NewAction;
			NewAction.BotAction = static_cast<EBotAction>(i);
			NewAction.QValue = 0.f;
			NewSec.Actions.Add(NewAction);
		}
		RLData.AllSections.Add(NewSec);
		Section = RLData.FindSection(SectionKey);
	}

	// 3. Epsilon-greedy
	EBotAction Action = EBotAction::None;
	if (FMath::FRand() < Epsilon)
	{
		Action = static_cast<EBotAction>(FMath::RandRange(1, 3)); // Avoid None
	}
	else
	{
		float MaxQ = -FLT_MAX;
		for (auto& Act : Section->Actions)
			if (Act.QValue > MaxQ)
			{
				MaxQ = Act.QValue;
				Action = Act.BotAction;
			}
	}

	// 4. Cache для ApplyReward
	LastSectionKey = SectionKey;
	LastAction = Action;

	// Додаємо крок епізоду (нагороди ще немає — додаємо Reward=0, пізніше оновимо)
	FEpisodeStep Step;
	Step.SectionKey = SectionKey;
	Step.Action = Action;
	Step.Reward = 0.f;
	CurrentEpisode.Add(Step);

	UDSimDebugComponent* DebugComponent = UDSimBlueprintFunctionLibrary::GetDebugComponent(GetWorld());
	if (IsValid(DebugComponent))
	{
		DebugComponent->UpdateSelectedAction(Action);

		float ScoreGoal = -1.0f;
		float ScoreCover = -1.0f;
		float ScoreRandom = -1.0f;
		
		for (const auto& Item : Section->Actions)
		{
			switch (Item.BotAction)
			{
			case EBotAction::RandomMove:
				ScoreRandom = Item.QValue;

			case EBotAction::TowardCover:
				ScoreCover = Item.QValue;

			case EBotAction::TowardGoal:
				ScoreGoal = Item.QValue;
			}
		}
		DebugComponent->UpdateScores(ScoreGoal, ScoreCover, ScoreRandom);
	}

	return Action;
}

void UDSimReinforcementLearningComp::ApplyReward(float Reward, bool bEpisodeEnd)
{
	// Додаємо нагороду до останнього кроку
	if (CurrentEpisode.Num() > 0)
		CurrentEpisode.Last().Reward = Reward;

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

void UDSimReinforcementLearningComp::OnEpisodeEnd()
{
	// 1. Backpropagate reward через trajectory (reverse)
	float CumulativeReward = 0.f;
	for (int32 i = CurrentEpisode.Num() - 1; i >= 0; --i)
	{
		CumulativeReward = CurrentEpisode[i].Reward + Gamma * CumulativeReward;

		FDSimRLSectionData* Section = RLData.FindSection(CurrentEpisode[i].SectionKey);
		if (Section)
		{
			FDSimBotActionData* ActionData = Section->FindAction(CurrentEpisode[i].Action);
			if (ActionData)
			{
				ActionData->QValue += Alpha * (CumulativeReward - ActionData->QValue);
				ActionData->QValue = FMath::Clamp(ActionData->QValue, -1.f, 1.f);
			}
		}
	}

	// 2. Replay buffer
	ReplayBuffer.Add(CurrentEpisode);
	if (ReplayBuffer.Num() > ReplayBufferSize)
		ReplayBuffer.RemoveAt(0);

	// 3. Save
	SaveRLDataToFile();

	// 4. Debug
	LogRLDebug(TEXT("Episode finished: reward propagated."));
}

void UDSimReinforcementLearningComp::StartNewEpisode()
{
	CurrentEpisode.Empty();
}

void UDSimReinforcementLearningComp::DecayEpsilon()
{
	Epsilon = FMath::Max(Epsilon * EpsilonDecay, EpsilonMin);
}

void UDSimReinforcementLearningComp::LogRLDebug(const FString& Msg)
{
	UE_LOG(LogTemp, Log, TEXT("[RL] %s | Epsilon=%.3f"), *Msg, Epsilon);
}

bool UDSimReinforcementLearningComp::SaveRLDataToFile()
{
	FString Json;
	const FString Path = GetRLDataSavePath();
	if (FJsonObjectConverter::UStructToJsonObjectString(RLData, Json))
		return FFileHelper::SaveStringToFile(Json, *Path);
	return false;
}

bool UDSimReinforcementLearningComp::LoadRLDataFromFile()
{
	const FString Path = GetRLDataSavePath();
	FString Json;
	if (FPaths::FileExists(Path) && FFileHelper::LoadFileToString(Json, *Path))
		return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &RLData, 0, 0);
	RLData.AllSections.Empty();
	return false;
}

FString UDSimReinforcementLearningComp::GetRLDataSavePath() const
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames/");
	IFileManager::Get().MakeDirectory(*Dir, true);
	return Dir / TEXT("RLData.json");
}
