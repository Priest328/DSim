// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DSimCharacterAIController.generated.h"

class ASphereActor;
class UDSimReinforcementLearningComp;
class ADSimCharacter;
class UBehaviorTreeComponent;

namespace AIBlackboardKeys {
	static const FName DroneActor = "DroneActor";
	static const FName GoalLocation = "GoalLocation";
	static const FName NearestCover = "NearestCover";
	static const FName MoveLocation = "MoveLocation";
	static const FName CurrentBotAction = "CurrentBotAction";
	static const FName IsSprinting = "IsSprinting";
}

/**
 * 
 */
UCLASS()
class DSIM_API ADSimCharacterAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	void OnDeath();

	UFUNCTION(BlueprintCallable, Category = "AI|Episode")
	void StopEpisodeLogic();

	UFUNCTION(BlueprintCallable, Category = "AI|Episode")
	void StartEpisodeLogic();
	
protected:
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void DrawLocations();

	ADSimCharacterAIController();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UDSimReinforcementLearningComp> RLComp;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ASphereActor> SphereMarkerClass;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TObjectPtr<UBlackboardData> BlackboardComponent;

private:
	UPROPERTY()
	TObjectPtr<ADSimCharacter> CharacterRef;

	// TODO: Move to EQS 
	void SetupDroneActor();

	TArray<FVector> Locations;

	FTimerHandle LocationUpdateTimer;
};
