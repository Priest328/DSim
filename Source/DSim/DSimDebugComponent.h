// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DSim/AI/ML/DSimReinforcementLearningComp.h"
#include "Components/ActorComponent.h"
#include "DSimDebugComponent.generated.h"

struct FDroneState;

USTRUCT(BlueprintType)
struct FPPOStateScores {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TowardGoalScore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TowardCoverScore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RandomMoveScore;
};

USTRUCT(BlueprintType)
struct FDebugData {
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LastReward = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction LastSelectedAction;

	// PPO Debug
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPPOStateScores ActionScores;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DSIM_API UDSimDebugComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UDSimDebugComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void UpdateSelectedAction(EBotAction Action);

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void UpdateReward(float Reward);

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void UpdateScores(float GoalScore, float CoverScore, float RandomScore);
	
	UFUNCTION(BlueprintPure, Category = "Debug")
	const FDebugData& GetDebugData() const;

private:
	FDebugData DebugInfo;
};
