#include "DSim/AI/Training/DSimSimulationManager.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Character/Drone/Components/DSimDroneTelemetryComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"

ADSimSimulationManager::ADSimSimulationManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADSimSimulationManager::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimes();
	SaveInitialSnapshots();

	if (bStartOnBeginPlay)
	{
		StartExperiment();
	}
}

void ADSimSimulationManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDrawDebugInfo)
	{
		DrawDebugInfo();
	}
}

void ADSimSimulationManager::StartExperiment()
{
	if (ExperimentState == EDSimExperimentState::Running)
	{
		return;
	}

	BuildRuntimes();
	SaveInitialSnapshots();

	ExperimentState = EDSimExperimentState::Running;

	for (int32 PairIndex = 0; PairIndex < PairConfigs.Num(); ++PairIndex)
	{
		if (!PairConfigs[PairIndex].bEnabled)
		{
			if (PairRuntimes.IsValidIndex(PairIndex))
			{
				PairRuntimes[PairIndex].bPairFinished = true;
			}
			continue;
		}

		EnsureAlgorithmForPair(PairIndex);
		ConfigureAlgorithmForPair(PairIndex);
		StartEpisodeForPair(PairIndex);
	}
}

void ADSimSimulationManager::StopExperiment()
{
	for (FDSimExperimentPairRuntime& Runtime : PairRuntimes)
	{
		GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);
		GetWorldTimerManager().ClearTimer(Runtime.NextEpisodeTimerHandle);
		Runtime.bEpisodeRunning = false;
		Runtime.bPairFinished = true;
	}

	ExperimentState = EDSimExperimentState::Finished;
}

void ADSimSimulationManager::StartEpisodeForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Config.bEnabled || Runtime.bPairFinished)
	{
		return;
	}

	if (Runtime.CompletedEpisodes >= Config.EpisodesCount)
	{
		Runtime.bPairFinished = true;

		if (AreAllPairsFinished())
		{
			StopExperiment();
		}

		return;
	}

	Runtime.CurrentEpisodeId = Runtime.CompletedEpisodes + 1;
	Runtime.bEpisodeRunning = true;

	// На всякий випадок зупиняємо попередню логіку перед reset.
	StopPairLogic(PairIndex);

	ResetActorFromSnapshot(Runtime.DroneSnapshot);
	ResetActorFromSnapshot(Runtime.BotSnapshot);

	ConfigureDroneForPair(PairIndex);

	const FString TrainingFile = BuildTrainingDataFileName(Config, Runtime.CurrentEpisodeId);

	if (Runtime.ActiveAlgorithm)
	{
		Runtime.ActiveAlgorithm->SetEpisodeSummaryContext(
			RunId,
			PairIndex,
			Config.ArenaId,
			Config.StateRepresentationMode,
			TrainingFile
		);

		Runtime.ActiveAlgorithm->StartEpisode(Runtime.CurrentEpisodeId);
	}

	StartPairLogic(PairIndex);

	GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);

	FTimerDelegate TimeoutDelegate;
	TimeoutDelegate.BindUObject(this, &ADSimSimulationManager::HandleEpisodeTimeout, PairIndex);

	GetWorldTimerManager().SetTimer(
		Runtime.TimeoutTimerHandle,
		TimeoutDelegate,
		Config.EpisodeTimeoutSeconds,
		false
	);
}

void ADSimSimulationManager::FinishEpisodeForPair(int32 PairIndex, EDSimEpisodeFinishReason FinishReason)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Runtime.bEpisodeRunning)
	{
		return;
	}

	Runtime.bEpisodeRunning = false;

	GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);

	StopPairLogic(PairIndex);

	if (Runtime.ActiveAlgorithm)
	{
		Runtime.ActiveAlgorithm->EndEpisode(FinishReason);
	}

	SavePairOutputs(PairIndex, FinishReason);

	Runtime.CompletedEpisodes++;

	if (Runtime.CompletedEpisodes >= Config.EpisodesCount)
	{
		Runtime.bPairFinished = true;

		if (AreAllPairsFinished())
		{
			StopExperiment();
		}

		return;
	}

	ScheduleNextEpisodeForPair(PairIndex);
}

void ADSimSimulationManager::NotifyBotReachedGoal(AActor* BotActor)
{
	const int32 PairIndex = FindPairIndexByBot(BotActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::GoalReached);
}

void ADSimSimulationManager::NotifyBotKilled(AActor* BotActor)
{
	const int32 PairIndex = FindPairIndexByBot(BotActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::BotKilledByDrone);
}

void ADSimSimulationManager::NotifyDroneCrashed(AActor* DroneActor)
{
	const int32 PairIndex = FindPairIndexByDrone(DroneActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::DroneCrashed);
}

void ADSimSimulationManager::BuildRuntimes()
{
	PairRuntimes.SetNum(PairConfigs.Num());

	for (FDSimExperimentPairRuntime& Runtime : PairRuntimes)
	{
		Runtime.bEpisodeRunning = false;
		Runtime.bPairFinished = false;
		Runtime.CompletedEpisodes = 0;
		Runtime.CurrentEpisodeId = 0;
		Runtime.ActiveAlgorithm = nullptr;
	}
}

void ADSimSimulationManager::SaveInitialSnapshots()
{
	for (int32 PairIndex = 0; PairIndex < PairConfigs.Num(); ++PairIndex)
	{
		if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
		{
			continue;
		}

		const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
		FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

		if (!Config.bEnabled)
		{
			continue;
		}

		SaveSnapshot(Runtime.DroneSnapshot, Config.Drone);
		SaveSnapshot(Runtime.BotSnapshot, Config.Bot);
	}
}

void ADSimSimulationManager::SaveSnapshot(FDSimActorSnapshot& Snapshot, AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	Snapshot.Actor = Actor;
	Snapshot.InitialTransform = Actor->GetActorTransform();
}

void ADSimSimulationManager::ResetActorFromSnapshot(const FDSimActorSnapshot& Snapshot) const
{
	if (!IsValid(Snapshot.Actor))
	{
		return;
	}

	Snapshot.Actor->SetActorTransform(
		Snapshot.InitialTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Snapshot.Actor->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	if (UMovementComponent* MovementComponent = Snapshot.Actor->FindComponentByClass<UMovementComponent>())
	{
		MovementComponent->StopMovementImmediately();
	}
}

UDSimBotTrainingAlgorithmComponent* ADSimSimulationManager::EnsureAlgorithmForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return nullptr;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!IsValid(Config.Bot))
	{
		return nullptr;
	}

	UDSimBotTrainingAlgorithmComponent* Existing =
		Config.Bot->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();

	UE_LOG(LogTemp, Warning,
	       TEXT("[SimManager] EnsureAlgorithmForPair Pair=%d Bot=%s AlgorithmClass=%s Existing=%s"),
	       PairIndex,
	       *GetNameSafe(Config.Bot),
	       *GetNameSafe(Config.AlgorithmClass),
	       *GetNameSafe(Existing)
	);

	if (IsValid(Existing))
	{
		if (!Config.AlgorithmClass || Existing->GetClass() == Config.AlgorithmClass)
		{
			Runtime.ActiveAlgorithm = Existing;
			return Existing;
		}

		Existing->DestroyComponent();
	}

	if (!Config.AlgorithmClass)
	{
		return nullptr;
	}

	UDSimBotTrainingAlgorithmComponent* NewAlgorithm =
		NewObject<UDSimBotTrainingAlgorithmComponent>(Config.Bot, Config.AlgorithmClass);

	if (!IsValid(NewAlgorithm))
	{
		return nullptr;
	}

	NewAlgorithm->RegisterComponent();
	Runtime.ActiveAlgorithm = NewAlgorithm;

	return NewAlgorithm;
}

void ADSimSimulationManager::ConfigureDroneForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	if (!IsValid(Config.Drone) || !IsValid(Config.Bot))
	{
		return;
	}

	Config.Drone->StopAutopilotLogic();
	Config.Drone->ResetAutopilotRuntime();

	Config.Drone->SetArenaId(Config.ArenaId);
	Config.Drone->SetTargetBot(Config.Bot);
	Config.Drone->SetDroneControlMode(Config.DroneMode);

	if (UDSimDroneTelemetryComponent* Telemetry =
		Config.Drone->FindComponentByClass<UDSimDroneTelemetryComponent>())
	{
		const int32 EpisodeId = PairRuntimes.IsValidIndex(PairIndex)
			                        ? PairRuntimes[PairIndex].CurrentEpisodeId
			                        : 0;

		Telemetry->ClearFrames();
		Telemetry->SetEpisodeId(EpisodeId);
	}

	Config.Drone->StartAutopilotLogic();
}

void ADSimSimulationManager::ConfigureAlgorithmForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Runtime.ActiveAlgorithm)
	{
		return;
	}

	FDSimAlgorithmRuntimeContext Context;
	Context.ArenaId = Config.ArenaId;
	Context.RunId = RunId;
	Context.StateRepresentationMode = Config.StateRepresentationMode;

	Context.InitialTrainingFile = Config.bLoadInitialTrainingFileOnExperimentStart
		                              ? Config.InitialTrainingFile
		                              : TEXT("");

	Context.OutputDirectory = BuildPairDirectory(Config);
	Context.OutputTrainingFile = BuildTrainingDataFileName(Config, 0);

	Runtime.ActiveAlgorithm->InitializeAlgorithm(Context, Config.AlgorithmConfig);
}

void ADSimSimulationManager::HandleEpisodeTimeout(int32 PairIndex)
{
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::Timeout);
}

void ADSimSimulationManager::ScheduleNextEpisodeForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];
	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	FTimerDelegate NextEpisodeDelegate;
	NextEpisodeDelegate.BindUObject(this, &ADSimSimulationManager::StartEpisodeForPair, PairIndex);

	GetWorldTimerManager().SetTimer(
		Runtime.NextEpisodeTimerHandle,
		NextEpisodeDelegate,
		Config.DelayBeforeNextEpisode,
		false
	);
}

void ADSimSimulationManager::SavePairOutputs(int32 PairIndex, EDSimEpisodeFinishReason FinishReason)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("[SimManager] SavePairOutputs failed: invalid PairIndex=%d"), PairIndex);
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	UE_LOG(LogTemp, Warning,
	       TEXT("[SimManager] SavePairOutputs Pair=%d Episode=%d OutputName=%s"),
	       PairIndex,
	       Runtime.CurrentEpisodeId,
	       *Config.OutputName
	);

	if (IsValid(Config.Drone))
	{
		if (UDSimDroneTelemetryComponent* Telemetry =
			Config.Drone->FindComponentByClass<UDSimDroneTelemetryComponent>())
		{
			const FString TelemetryFile = BuildTelemetryFileName(Config, Runtime.CurrentEpisodeId);

			const bool bTelemetrySaved = Telemetry->SaveTelemetryToCsv(TelemetryFile);

			UE_LOG(LogTemp, Warning,
			       TEXT("[SimManager] Telemetry save: %s | Path=%s"),
			       bTelemetrySaved ? TEXT("OK") : TEXT("FAILED"),
			       *TelemetryFile
			);
		}
	}

	if (!Config.bSaveTrainingDataAfterEachEpisode)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[SimManager] Training data save skipped: bSaveTrainingDataAfterEachEpisode=false"));
		return;
	}

	if (!IsValid(Runtime.ActiveAlgorithm))
	{
		UE_LOG(LogTemp, Error, TEXT("[SimManager] Training data save failed: ActiveAlgorithm is null"));
		return;
	}

	const FString TrainingFile = BuildTrainingDataFileName(Config, Runtime.CurrentEpisodeId);

	const bool bTrainingSaved = Runtime.ActiveAlgorithm->SaveTrainingData(TrainingFile);

	AppendEpisodeSummary(PairIndex, FinishReason);
	
	UE_LOG(LogTemp, Warning,
	       TEXT("[SimManager] Training data save: %s | Algorithm=%s | Path=%s"),
	       bTrainingSaved ? TEXT("OK") : TEXT("FAILED"),
	       *Runtime.ActiveAlgorithm->GetAlgorithmName(),
	       *TrainingFile
	);
}

int32 ADSimSimulationManager::FindPairIndexByBot(AActor* BotActor) const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (PairConfigs[i].Bot == BotActor)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 ADSimSimulationManager::FindPairIndexByDrone(AActor* DroneActor) const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (PairConfigs[i].Drone == DroneActor)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

bool ADSimSimulationManager::AreAllPairsFinished() const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (!PairConfigs[i].bEnabled)
		{
			continue;
		}

		if (!PairRuntimes.IsValidIndex(i) || !PairRuntimes[i].bPairFinished)
		{
			return false;
		}
	}

	return true;
}

FString ADSimSimulationManager::BuildRunDirectory() const
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Experiments") /
		FString::Printf(TEXT("Run_%03d"), RunId);

	return FPaths::ConvertRelativePathToFull(Path);
}

FString ADSimSimulationManager::BuildPairDirectory(const FDSimExperimentPairConfig& Config) const
{
	const FString Path = BuildRunDirectory() /
		FString::Printf(TEXT("Arena_%02d_%s"), Config.ArenaId, *Config.OutputName);

	return FPaths::ConvertRelativePathToFull(Path);
}

FString ADSimSimulationManager::BuildTrainingDataFileName(
	const FDSimExperimentPairConfig& Config,
	int32 EpisodeId
) const
{
	const FString Directory = BuildPairDirectory(Config);
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FString FileName = EpisodeId <= 0
		                         ? TEXT("training_final.json")
		                         : FString::Printf(TEXT("training_ep_%03d.json"), EpisodeId);

	return FPaths::ConvertRelativePathToFull(Directory / FileName);
}

FString ADSimSimulationManager::BuildTelemetryFileName(
	const FDSimExperimentPairConfig& Config,
	int32 EpisodeId
) const
{
	const FString Directory = BuildPairDirectory(Config);
	IFileManager::Get().MakeDirectory(*Directory, true);

	return FString::Printf(
		TEXT("telemetry_ep_%03d.csv"),
		EpisodeId
	);
}

void ADSimSimulationManager::DrawDebugInfo() const
{
	if (!GEngine)
	{
		return;
	}

	int32 RunningPairs = 0;
	int32 FinishedPairs = 0;
	int32 CompletedEpisodes = 0;
	int32 TargetEpisodes = 0;

	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (!PairConfigs[i].bEnabled)
		{
			continue;
		}

		TargetEpisodes += PairConfigs[i].EpisodesCount;

		if (PairRuntimes.IsValidIndex(i))
		{
			CompletedEpisodes += PairRuntimes[i].CompletedEpisodes;

			if (PairRuntimes[i].bEpisodeRunning)
			{
				RunningPairs++;
			}

			if (PairRuntimes[i].bPairFinished)
			{
				FinishedPairs++;
			}
		}
	}

	const FString Text = FString::Printf(
		TEXT("Experiment: %s | Run: %03d | Episodes: %d / %d | Running pairs: %d | Finished pairs: %d"),
		*UEnum::GetValueAsString(ExperimentState),
		RunId,
		CompletedEpisodes,
		TargetEpisodes,
		RunningPairs,
		FinishedPairs
	);

	GEngine->AddOnScreenDebugMessage(
		987654,
		DebugTextDuration,
		FColor::Green,
		Text
	);
}

void ADSimSimulationManager::StartPairLogic(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	// Start bot BT logic
	if (IsValid(Config.Bot))
	{
		if (ADSimCharacterAIController* AICon = Cast<ADSimCharacterAIController>(Config.Bot->GetController()))
		{
			AICon->StartEpisodeLogic();
		}
	}

	// Start drone State Tree logic
	if (IsValid(Config.Drone))
	{
		Config.Drone->StartAutopilotLogic();
	}
}

void ADSimSimulationManager::StopPairLogic(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	// Stop bot BT logic
	if (IsValid(Config.Bot))
	{
		if (ADSimCharacterAIController* AICon = Cast<ADSimCharacterAIController>(Config.Bot->GetController()))
		{
			AICon->StopEpisodeLogic();
		}
	}

	// Stop drone State Tree logic
	if (IsValid(Config.Drone))
	{
		Config.Drone->StopAutopilotLogic();
	}
}

FString ADSimSimulationManager::BuildEpisodeSummaryFileName(
	const FDSimExperimentPairConfig& Config
) const
{
	const FString Directory = BuildPairDirectory(Config);
	IFileManager::Get().MakeDirectory(*Directory, true);

	return FPaths::ConvertRelativePathToFull(
		Directory / TEXT("episode_summary.csv")
	);
}

void ADSimSimulationManager::AppendEpisodeSummary(
	int32 PairIndex,
	EDSimEpisodeFinishReason FinishReason
)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	const FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!IsValid(Runtime.ActiveAlgorithm))
	{
		return;
	}

	const FDSimEpisodeSummary& Summary =
		Runtime.ActiveAlgorithm->GetCurrentEpisodeSummary();

	const FString SummaryFile = BuildEpisodeSummaryFileName(Config);
	const bool bFileAlreadyExists = FPaths::FileExists(SummaryFile);

	FString Csv;

	if (!bFileAlreadyExists)
	{
		Csv += TEXT("RunId,PairIndex,ArenaId,EpisodeId,AlgorithmName,StateMode,FinishReason,TotalReward,EpisodeDuration,TowardGoalCount,TowardCoverCount,RandomMoveCount,OutputTrainingFile\n");
	}

	const FString StateModeString = UEnum::GetValueAsString(Summary.StateRepresentationMode);
	const FString FinishReasonString = UEnum::GetValueAsString(FinishReason);

	Csv += FString::Printf(
		TEXT("%d,%d,%d,%d,%s,%s,%s,%.4f,%.4f,%d,%d,%d,%s\n"),
		Summary.RunId,
		Summary.PairIndex,
		Summary.ArenaId,
		Summary.EpisodeId,
		*Summary.AlgorithmName,
		*StateModeString,
		*FinishReasonString,
		Summary.TotalReward,
		Summary.EpisodeDuration,
		Summary.TowardGoalCount,
		Summary.TowardCoverCount,
		Summary.RandomMoveCount,
		*Summary.OutputTrainingFile
	);

	FFileHelper::SaveStringToFile(
		Csv,
		*SummaryFile,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		bFileAlreadyExists ? FILEWRITE_Append : 0
	);

	UE_LOG(LogTemp, Warning,
		TEXT("[SimManager] Episode summary saved: %s"),
		*SummaryFile
	);
}