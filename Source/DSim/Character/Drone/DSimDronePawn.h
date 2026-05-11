// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DSimDronePawn.generated.h"

class ASphereActor;
class ADSimCharacter;

class USphereComponent;
class USpringArmComponent;
class UBoxComponent;
class UCameraComponent;
class UFloatingPawnMovement;
class UStateTreeComponent;

class UDSimDronePerceptionComponent;
class UDSimDroneFlightNavigationComponent;
class UDSimDroneTelemetryComponent;

struct FInputActionValue;
class UInputMappingContext;
class UInputAction;

UENUM(BlueprintType)
enum class EDroneControlMode : uint8
{
	HumanControlled     UMETA(DisplayName = "Human Controlled"),
	SimplePursuit       UMETA(DisplayName = "Simple Pursuit"),
	PredictivePursuit   UMETA(DisplayName = "Predictive Pursuit"),
	RandomizedAttack    UMETA(DisplayName = "Randomized Attack"),
	RecordedReplay      UMETA(DisplayName = "Recorded Replay")
};

UENUM(BlueprintType)
enum class EDroneAttackDiveResult : uint8
{
	NotStarted UMETA(DisplayName = "Not Started"),
	InProgress UMETA(DisplayName = "In Progress"),
	Impact     UMETA(DisplayName = "Impact"),
	Blocked    UMETA(DisplayName = "Blocked"),
	Failed     UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EDroneAutopilotState : uint8
{
	Idle                UMETA(DisplayName = "Idle"),
	AcquireTarget       UMETA(DisplayName = "Acquire Target"),
	Pursuit             UMETA(DisplayName = "Pursuit"),
	PredictivePursuit   UMETA(DisplayName = "Predictive Pursuit"),
	RandomizedAttack    UMETA(DisplayName = "Randomized Attack"),
	AvoidObstacle       UMETA(DisplayName = "Avoid Obstacle"),
	AttackRun           UMETA(DisplayName = "Attack Run"),
	Search              UMETA(DisplayName = "Search"),
	Stopped             UMETA(DisplayName = "Stopped")
};

UCLASS()
class DSIM_API ADSimDronePawn : public APawn
{
	GENERATED_BODY()

public:
	ADSimDronePawn();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnDroneOverlapped(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

private:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void HandleThrottle(const FInputActionValue& Value);
	void HandleYaw(const FInputActionValue& Value);
	void HandlePitch(const FInputActionValue& Value);
	void HandleRoll(const FInputActionValue& Value);

	void ResetPitch(const FInputActionValue&);
	void ResetRoll(const FInputActionValue&);
	void ResetYaw(const FInputActionValue&);
	void ResetThrottle(const FInputActionValue&);

	void HandleMovementFromInput(float DeltaTime);
	void DroneExplode();

	UFUNCTION()
	void StopLogic();

	UFUNCTION()
	void DrawLocations();

	UFUNCTION()
	void OnDroneHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

public:
	// -----------------------------
	// State Tree / Autopilot API
	// -----------------------------

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void SetTargetBot(AActor* InTargetBot);

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	AActor* GetTargetBot() const { return TargetBotActor; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void SetDroneControlMode(EDroneControlMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	EDroneControlMode GetDroneControlMode() const { return DroneControlMode; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void SetAutopilotState(EDroneAutopilotState NewState);

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	EDroneAutopilotState GetAutopilotState() const { return AutopilotState; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void SetDesiredFlightTarget(const FVector& NewTarget);

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	FVector GetDesiredFlightTarget() const { return DesiredFlightTarget; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void SetSafeFlightTarget(const FVector& NewTarget);

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	FVector GetSafeFlightTarget() const { return SafeFlightTarget; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void ApplyAutopilotMovement(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	bool IsAutopilotEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	bool HasValidTargetBot() const;

	UFUNCTION(BlueprintPure, Category = "Drone|Autopilot")
	int32 GetArenaId() const { return ArenaId; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Autopilot")
	void MoveAutopilotTowardsSafeTarget(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Drone|Attack")
	bool TryPrepareAttackDive();

	UFUNCTION(BlueprintCallable, Category = "Drone|Attack")
	EDroneAttackDiveResult TickAttackDive(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Drone|Attack")
	void AbortAttackDive();

	UFUNCTION(BlueprintPure, Category = "Drone|Attack")
	bool IsAttackDiveActive() const { return bAttackDiveActive; }

	UFUNCTION(BlueprintPure, Category = "Drone|Attack")
	FVector GetAttackDiveTarget() const { return AttackDiveTarget; }

public:
	UPROPERTY(EditAnywhere, Category = "Drone|Control")
	bool bSmoothStabilization = true;

	UPROPERTY(EditAnywhere, Category = "Drone|Control")
	float InputReturnSpeed = 3.f;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ASphereActor> SphereMarkerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	EDroneControlMode DroneControlMode = EDroneControlMode::HumanControlled;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	int32 ArenaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float AutopilotSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float AutopilotAccelerationInterpSpeed = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float AutopilotRotationInterpSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float AttackRange = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float PredictionTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drone|Autopilot")
	float RandomAttackOffset = 450.f;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DroneMappingContext;

	UPROPERTY(EditAnywhere, Category = "Drone|Autopilot")
	TObjectPtr<AActor> TargetBotActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Throttle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Yaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Roll;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UBoxComponent> BoxComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UFloatingPawnMovement> FloatingComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<USphereComponent> ExplodeSphereComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component|StateTree")
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component|Drone AI")
	TObjectPtr<UDSimDronePerceptionComponent> DronePerceptionComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component|Drone AI")
	TObjectPtr<UDSimDroneFlightNavigationComponent> DroneFlightNavigationComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component|Drone AI")
	TObjectPtr<UDSimDroneTelemetryComponent> DroneTelemetryComponent;

private:
	// Input
	float ThrottleInput = 0.f;
	float YawInput = 0.f;
	float PitchInput = 0.f;
	float RollInput = 0.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotation")
	float MaxPitchAngle = 45.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotation")
	float MaxRollAngle = 45.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotation")
	float RotationSpeed = 30.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Movement")
	float MoveSpeed = 1000.f;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Autopilot")
	EDroneAutopilotState AutopilotState = EDroneAutopilotState::Idle;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Autopilot")
	FVector DesiredFlightTarget = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Autopilot")
	FVector SafeFlightTarget = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Autopilot")
	FVector CurrentAutopilotVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Autopilot")
	float LastAttackTime = -1000.f;

	TArray<FVector> Locations;

	// TODO: Delete later. Only for debug purposes.
	FTimerHandle LocationUpdateTimer;

	

private:
	UPROPERTY(EditAnywhere, Category = "Drone|Attack")
	float AttackDiveStartRange = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack")
	float AttackDiveSpeed = 2200.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack")
	float AttackDiveAccelerationInterpSpeed = 4.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack")
	float AttackDiveRotationInterpSpeed = 7.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack")
	float AttackImpactRadius = 180.f;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Attack")
	bool bAttackDiveActive = false;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Attack")
	FVector AttackDiveTarget = FVector::ZeroVector;
};