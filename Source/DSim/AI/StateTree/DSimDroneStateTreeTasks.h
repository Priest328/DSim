#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "DSimDroneStateTreeTasks.generated.h"

class ADSimDronePawn;
class UDSimDronePerceptionComponent;
class UDSimDroneFlightNavigationComponent;
class UDSimDroneTelemetryComponent;

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories)
class DSIM_API UDSimDroneStateTreeTaskBase : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

public:
	UDSimDroneStateTreeTaskBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context")
	TObjectPtr<ADSimDronePawn> DronePawn;

	ADSimDronePawn* GetDronePawn() const;
	UDSimDronePerceptionComponent* GetPerceptionComponent() const;
	UDSimDroneFlightNavigationComponent* GetFlightNavigationComponent() const;
	UDSimDroneTelemetryComponent* GetTelemetryComponent() const;
};

// ------------------------------------------------------------
// Global task: Update perception.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Update Drone Perception"))
class DSIM_API UDSimSTTask_UpdateDronePerception : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_UpdateDronePerception(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
};

// ------------------------------------------------------------
// Global task: Capture telemetry.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Capture Drone Telemetry"))
class DSIM_API UDSimSTTask_CaptureDroneTelemetry : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_CaptureDroneTelemetry(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
};

// ------------------------------------------------------------
// Select desired target based on DroneControlMode.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Select Desired Flight Target"))
class DSIM_API UDSimSTTask_SelectDesiredFlightTarget : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_SelectDesiredFlightTarget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;

private:
	bool UpdateDesiredTarget();

	UPROPERTY(EditAnywhere, Category = "Randomized Attack")
	float RandomTargetRefreshTime = 1.0f;

	UPROPERTY(Transient)
	FVector CachedRandomTarget = FVector::ZeroVector;

	UPROPERTY(Transient)
	float LastRandomTargetTime = -1000.f;
};

// ------------------------------------------------------------
// Find safe 3D target from DesiredFlightTarget.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Find Safe Flight Target"))
class DSIM_API UDSimSTTask_FindSafeFlightTarget : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_FindSafeFlightTarget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;

private:
	bool UpdateSafeTarget();
};

// ------------------------------------------------------------
// Move drone to SafeFlightTarget.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Follow Safe Flight Target"))
class DSIM_API UDSimSTTask_FollowSafeFlightTarget : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_FollowSafeFlightTarget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
};

// ------------------------------------------------------------
// Attack target if possible.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Attack Dive Target"))
class DSIM_API UDSimSTTask_AttackDiveTarget : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_AttackDiveTarget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		const float DeltaTime
	) override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) override;
};

// ------------------------------------------------------------
// Search mode: move to last known bot location.
// ------------------------------------------------------------

UCLASS(Blueprintable, EditInlineNew, CollapseCategories, meta = (DisplayName = "DSim Move To Last Known Target Location"))
class DSIM_API UDSimSTTask_MoveToLastKnownTargetLocation : public UDSimDroneStateTreeTaskBase
{
	GENERATED_BODY()

public:
	UDSimSTTask_MoveToLastKnownTargetLocation(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;

private:
	bool UpdateLastKnownTarget();
};