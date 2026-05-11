#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSim/AI/Training/DSimTrainingTypes.h"
#include "DSimBotTrainingAlgorithmComponent.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class DSIM_API UDSimBotTrainingAlgorithmConfig : public UObject
{
	GENERATED_BODY()
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class DSIM_API UDSimTabularRLAlgorithmConfig : public UDSimBotTrainingAlgorithmConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL")
	float LearningRate = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL")
	float DiscountFactor = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL")
	float EpsilonStart = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL")
	float EpsilonMin = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RL")
	float EpsilonDecay = 0.995f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	int32 HorizontalSections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	int32 VerticalLanes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	bool bUseThreatFeatures = false;
};

UCLASS(Abstract, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DSIM_API UDSimBotTrainingAlgorithmComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimBotTrainingAlgorithmComponent();

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual void InitializeAlgorithm(
		const FDSimAlgorithmRuntimeContext& InContext,
		UDSimBotTrainingAlgorithmConfig* InConfig
	);
	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual void SetGoalPosition(const FVector& NewGoalPosition);

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual void StartEpisode(int32 EpisodeId);

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual void EndEpisode(EDSimEpisodeFinishReason FinishReason);

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual EBotAction RequestTrainingAction();

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual void AddTrainingReward(float Reward);

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual bool LoadTrainingData(const FString& FileName);

	UFUNCTION(BlueprintCallable, Category = "Training Algorithm")
	virtual bool SaveTrainingData(const FString& FileName);

	UFUNCTION(BlueprintPure, Category = "Training Algorithm")
	virtual FString GetAlgorithmName() const;

	UFUNCTION(BlueprintPure, Category = "Training Algorithm")
	int32 GetCurrentEpisodeId() const { return CurrentEpisodeId; }

	UFUNCTION(BlueprintPure, Category = "Training Algorithm")
	const FDSimAlgorithmRuntimeContext& GetRuntimeContext() const { return RuntimeContext; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Algorithm")
	int32 CurrentEpisodeId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Algorithm")
	FDSimAlgorithmRuntimeContext RuntimeContext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Algorithm")
	TObjectPtr<UDSimBotTrainingAlgorithmConfig> AlgorithmConfig = nullptr;
};