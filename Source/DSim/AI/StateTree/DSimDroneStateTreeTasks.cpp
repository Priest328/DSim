#include "DSim/AI/StateTree/DSimDroneStateTreeTasks.h"

#include "StateTreeExecutionContext.h"
#include "DSim/Character/Drone/DSimDronePawn.h"
#include "DSim/Character/Drone/Components/DSimDroneFlightNavigationComponent.h"
#include "DSim/Character/Drone/Components/DSimDronePerceptionComponent.h"
#include "DSim/Character/Drone/Components/DSimDroneTelemetryComponent.h"

// ------------------------------------------------------------
// Base
// ------------------------------------------------------------

UDSimDroneStateTreeTaskBase::UDSimDroneStateTreeTaskBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = false;
}

ADSimDronePawn* UDSimDroneStateTreeTaskBase::GetDronePawn() const
{
	if (IsValid(DronePawn))
	{
		return DronePawn;
	}

	return nullptr;
}

UDSimDronePerceptionComponent* UDSimDroneStateTreeTaskBase::GetPerceptionComponent() const
{
	ADSimDronePawn* Pawn = GetDronePawn();
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UDSimDronePerceptionComponent>() : nullptr;
}

UDSimDroneFlightNavigationComponent* UDSimDroneStateTreeTaskBase::GetFlightNavigationComponent() const
{
	ADSimDronePawn* Pawn = GetDronePawn();
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UDSimDroneFlightNavigationComponent>() : nullptr;
}

UDSimDroneTelemetryComponent* UDSimDroneStateTreeTaskBase::GetTelemetryComponent() const
{
	ADSimDronePawn* Pawn = GetDronePawn();
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UDSimDroneTelemetryComponent>() : nullptr;
}

// ------------------------------------------------------------
// Update Perception
// ------------------------------------------------------------

UDSimSTTask_UpdateDronePerception::UDSimSTTask_UpdateDronePerception(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_UpdateDronePerception::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	if (UDSimDronePerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->UpdatePerception();
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus UDSimSTTask_UpdateDronePerception::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	if (UDSimDronePerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->UpdatePerception();
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

// ------------------------------------------------------------
// Capture Telemetry
// ------------------------------------------------------------

UDSimSTTask_CaptureDroneTelemetry::UDSimSTTask_CaptureDroneTelemetry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_CaptureDroneTelemetry::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	if (UDSimDroneTelemetryComponent* Telemetry = GetTelemetryComponent())
	{
		Telemetry->CaptureFrame();
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus UDSimSTTask_CaptureDroneTelemetry::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	if (UDSimDroneTelemetryComponent* Telemetry = GetTelemetryComponent())
	{
		Telemetry->CaptureFrame();
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

// ------------------------------------------------------------
// Select Desired Flight Target
// ------------------------------------------------------------

UDSimSTTask_SelectDesiredFlightTarget::UDSimSTTask_SelectDesiredFlightTarget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_SelectDesiredFlightTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	return UpdateDesiredTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus UDSimSTTask_SelectDesiredFlightTarget::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	return UpdateDesiredTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

bool UDSimSTTask_SelectDesiredFlightTarget::UpdateDesiredTarget()
{
	ADSimDronePawn* Pawn = GetDronePawn();
	UDSimDronePerceptionComponent* Perception = GetPerceptionComponent();

	if (!IsValid(Pawn) || !IsValid(Perception) || !Pawn->HasValidTargetBot())
	{
		return false;
	}

	const FDronePerceptionSnapshot& Snapshot = Perception->GetSnapshot();

	FVector DesiredTarget = Snapshot.TargetLocation;

	switch (Pawn->GetDroneControlMode())
	{
	case EDroneControlMode::SimplePursuit:
		Pawn->SetAutopilotState(EDroneAutopilotState::Pursuit);
		DesiredTarget = Snapshot.TargetLocation;
		break;

	case EDroneControlMode::PredictivePursuit:
		Pawn->SetAutopilotState(EDroneAutopilotState::PredictivePursuit);
		DesiredTarget = Snapshot.TargetLocation + Snapshot.TargetVelocity * Pawn->PredictionTime;
		break;

	case EDroneControlMode::RandomizedAttack:
		{
			Pawn->SetAutopilotState(EDroneAutopilotState::RandomizedAttack);

			const float CurrentTime = Pawn->GetWorld() ? Pawn->GetWorld()->GetTimeSeconds() : 0.f;

			if (CachedRandomTarget.IsNearlyZero() || CurrentTime - LastRandomTargetTime >= RandomTargetRefreshTime)
			{
				const FVector RandomOffset = FVector(
					FMath::RandRange(-Pawn->RandomAttackOffset, Pawn->RandomAttackOffset),
					FMath::RandRange(-Pawn->RandomAttackOffset, Pawn->RandomAttackOffset),
					FMath::RandRange(0.f, Pawn->RandomAttackOffset)
				);

				CachedRandomTarget = Snapshot.TargetLocation + RandomOffset;
				LastRandomTargetTime = CurrentTime;
			}

			DesiredTarget = CachedRandomTarget;
			break;
		}

	case EDroneControlMode::RecordedReplay:
		// Later: replay will set DesiredTarget from recorded trajectory.
		// For now fallback to simple pursuit.
		Pawn->SetAutopilotState(EDroneAutopilotState::Pursuit);
		DesiredTarget = Snapshot.TargetLocation;
		break;

	case EDroneControlMode::HumanControlled:
	default:
		return false;
	}

	Pawn->SetDesiredFlightTarget(DesiredTarget);

	return true;
}

// ------------------------------------------------------------
// Find Safe Flight Target
// ------------------------------------------------------------

UDSimSTTask_FindSafeFlightTarget::UDSimSTTask_FindSafeFlightTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_FindSafeFlightTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	return UpdateSafeTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus UDSimSTTask_FindSafeFlightTarget::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	return UpdateSafeTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

bool UDSimSTTask_FindSafeFlightTarget::UpdateSafeTarget()
{
	ADSimDronePawn* Pawn = GetDronePawn();
	UDSimDroneFlightNavigationComponent* FlightNavigation = GetFlightNavigationComponent();

	if (!IsValid(Pawn) || !IsValid(FlightNavigation))
	{
		return false;
	}

	const FVector DesiredTarget = Pawn->GetDesiredFlightTarget();

	if (DesiredTarget.IsNearlyZero())
	{
		return false;
	}

	FVector SafeTarget = DesiredTarget;

	const bool bFoundSafeTarget = FlightNavigation->FindSafeFlightTarget(
		DesiredTarget,
		SafeTarget
	);

	Pawn->SetSafeFlightTarget(SafeTarget);

	return bFoundSafeTarget;
}

// ------------------------------------------------------------
// Follow Safe Flight Target
// ------------------------------------------------------------

UDSimSTTask_FollowSafeFlightTarget::UDSimSTTask_FollowSafeFlightTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_FollowSafeFlightTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	ADSimDronePawn* Pawn = GetDronePawn();

	if (!IsValid(Pawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus UDSimSTTask_FollowSafeFlightTarget::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	ADSimDronePawn* Pawn = GetDronePawn();

	if (!IsValid(Pawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	Pawn->MoveAutopilotTowardsSafeTarget(DeltaTime);

	return EStateTreeRunStatus::Running;
}

// ------------------------------------------------------------
// Attack Target If Possible
// ------------------------------------------------------------
UDSimSTTask_AttackDiveTarget::UDSimSTTask_AttackDiveTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_AttackDiveTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	ADSimDronePawn* Pawn = GetDronePawn();
	UE_LOG(LogTemp, Warning, TEXT("AttackDiveTask ENTER"));

	if (!IsValid(Pawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	Pawn->SetAutopilotState(EDroneAutopilotState::AttackRun);

	const bool bPrepared = Pawn->TryPrepareAttackDive();

	if (!bPrepared)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus UDSimSTTask_AttackDiveTarget::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	ADSimDronePawn* Pawn = GetDronePawn();
	UE_LOG(LogTemp, Warning, TEXT("AttackDiveTask TICK DeltaTime=%.4f"), DeltaTime);

	if (!IsValid(Pawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!Pawn->IsAttackDiveActive())
	{
		const bool bPrepared = Pawn->TryPrepareAttackDive();

		if (!bPrepared)
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	const EDroneAttackDiveResult Result = Pawn->TickAttackDive(DeltaTime);

	switch (Result)
	{
	case EDroneAttackDiveResult::InProgress:
		return EStateTreeRunStatus::Running;

	case EDroneAttackDiveResult::Impact:
		return EStateTreeRunStatus::Succeeded;

	case EDroneAttackDiveResult::Blocked:
	case EDroneAttackDiveResult::Failed:
	case EDroneAttackDiveResult::NotStarted:
	default:
		return EStateTreeRunStatus::Failed;
	}
}

void UDSimSTTask_AttackDiveTarget::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	UE_LOG(LogTemp, Warning, TEXT("AttackDiveTask EXIT"));
	// ADSimDronePawn* Pawn = GetDronePawn();
	//
	// if (IsValid(Pawn) && Pawn->IsAttackDiveActive())
	// {
	// 	Pawn->AbortAttackDive();
	// }
}

// ------------------------------------------------------------
// Move To Last Known Target Location
// ------------------------------------------------------------

UDSimSTTask_MoveToLastKnownTargetLocation::UDSimSTTask_MoveToLastKnownTargetLocation(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldCallTick = true;
}

EStateTreeRunStatus UDSimSTTask_MoveToLastKnownTargetLocation::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition
)
{
	return UpdateLastKnownTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus UDSimSTTask_MoveToLastKnownTargetLocation::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime
)
{
	return UpdateLastKnownTarget() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

bool UDSimSTTask_MoveToLastKnownTargetLocation::UpdateLastKnownTarget()
{
	ADSimDronePawn* Pawn = GetDronePawn();
	UDSimDronePerceptionComponent* Perception = GetPerceptionComponent();

	if (!IsValid(Pawn) || !IsValid(Perception))
	{
		return false;
	}

	const FVector LastKnownLocation = Perception->GetLastKnownTargetLocation();

	if (LastKnownLocation.IsNearlyZero())
	{
		return false;
	}

	Pawn->SetAutopilotState(EDroneAutopilotState::Search);
	Pawn->SetDesiredFlightTarget(LastKnownLocation);

	return true;
}
