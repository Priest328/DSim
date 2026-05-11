// Fill out your copyright notice in the Description page of Project Settings.

#include "DSim/Character/Drone/DSimDronePawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DSim/Actors/SphereActor.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Character/Drone/Components/DSimDroneFlightNavigationComponent.h"
#include "DSim/Character/Drone/Components/DSimDronePerceptionComponent.h"
#include "DSim/Character/Drone/Components/DSimDroneTelemetryComponent.h"
#include "DSim/Game/DSimGameMode.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "DSim/Player/DSimDroneController.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StateTreeComponent.h"
#include "Kismet/GameplayStatics.h"

ADSimDronePawn::ADSimDronePawn()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("CollisionComp");
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	FloatingComponent = CreateDefaultSubobject<UFloatingPawnMovement>("MovementComp");
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArmComponent");
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComp");
	ExplodeSphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComp");

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>("DroneStateTreeComp");
	DronePerceptionComponent = CreateDefaultSubobject<UDSimDronePerceptionComponent>("DronePerceptionComp");
	DroneFlightNavigationComponent = CreateDefaultSubobject<UDSimDroneFlightNavigationComponent>("DroneFlightNavigationComp");
	DroneTelemetryComponent = CreateDefaultSubobject<UDSimDroneTelemetryComponent>("DroneTelemetryComp");

	SetRootComponent(BoxComponent);

	MeshComponent->SetupAttachment(GetRootComponent());
	SpringArmComponent->SetupAttachment(MeshComponent);
	CameraComponent->SetupAttachment(SpringArmComponent);

	ExplodeSphereComponent->SetupAttachment(GetRootComponent());
	ExplodeSphereComponent->SetSphereRadius(1000.0f);

	BoxComponent->SetSimulatePhysics(false);
	BoxComponent->SetEnableGravity(false);
	BoxComponent->SetNotifyRigidBodyCollision(true);

	PrimaryActorTick.bCanEverTick = true;
}

void ADSimDronePawn::BeginPlay()
{
	Super::BeginPlay();

	BoxComponent->OnComponentHit.AddDynamic(this, &ADSimDronePawn::OnDroneHit);
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &ADSimDronePawn::OnDroneOverlapped);

	if (IsValid(DronePerceptionComponent))
	{
		DronePerceptionComponent->SetTargetBot(TargetBotActor);
	}

	ADSimGameMode* GameMode = Cast<ADSimGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (IsValid(GameMode))
	{
		GameMode->OnStopAllCharacters.AddDynamic(this, &ADSimDronePawn::StopLogic);
		GameMode->OnNeedToDrawDebugSpheres.AddDynamic(this, &ADSimDronePawn::DrawLocations);
	}

	GetWorldTimerManager().SetTimer(LocationUpdateTimer, [this]()
	{
		Locations.Add(GetActorLocation());

		if (!SphereMarkerClass)
		{
			return;
		}

		const FVector AdjustedPoint = GetActorLocation();

		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		GetWorld()->SpawnActor<ASphereActor>(
			SphereMarkerClass,
			AdjustedPoint,
			FRotator::ZeroRotator,
			Params
		);
	}, 0.5f, true);
}

void ADSimDronePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bSmoothStabilization)
	{
		PitchInput = FMath::FInterpTo(PitchInput, 0.f, DeltaTime, InputReturnSpeed);
		RollInput = FMath::FInterpTo(RollInput, 0.f, DeltaTime, InputReturnSpeed);
		YawInput = FMath::FInterpTo(YawInput, 0.f, DeltaTime, InputReturnSpeed);
		ThrottleInput = FMath::FInterpTo(ThrottleInput, 0.f, DeltaTime, InputReturnSpeed);
	}

	if (DroneControlMode == EDroneControlMode::HumanControlled)
	{
		HandleMovementFromInput(DeltaTime);
	}
}
void ADSimDronePawn::SetTargetBot(AActor* InTargetBot)
{
	TargetBotActor = InTargetBot;

	if (IsValid(DronePerceptionComponent))
	{
		DronePerceptionComponent->SetTargetBot(InTargetBot);
	}
}

void ADSimDronePawn::SetDroneControlMode(EDroneControlMode NewMode)
{
	DroneControlMode = NewMode;

	if (DroneControlMode == EDroneControlMode::HumanControlled)
	{
		SetAutopilotState(EDroneAutopilotState::Idle);
		CurrentAutopilotVelocity = FVector::ZeroVector;
	}
	else
	{
		SetAutopilotState(EDroneAutopilotState::AcquireTarget);
	}
}

void ADSimDronePawn::SetAutopilotState(EDroneAutopilotState NewState)
{
	AutopilotState = NewState;
}

void ADSimDronePawn::SetDesiredFlightTarget(const FVector& NewTarget)
{
	DesiredFlightTarget = NewTarget;
}

void ADSimDronePawn::SetSafeFlightTarget(const FVector& NewTarget)
{
	SafeFlightTarget = NewTarget;
}

bool ADSimDronePawn::IsAutopilotEnabled() const
{
	return DroneControlMode != EDroneControlMode::HumanControlled
		&& AutopilotState != EDroneAutopilotState::Stopped;
}

bool ADSimDronePawn::HasValidTargetBot() const
{
	return IsValid(TargetBotActor);
}

void ADSimDronePawn::ApplyAutopilotMovement(float DeltaTime)
{
	if (!HasValidTargetBot())
	{
		return;
	}

	FVector TargetToUse = DesiredFlightTarget;

	if (TargetToUse.IsNearlyZero())
	{
		TargetToUse = TargetBotActor->GetActorLocation();
	}

	if (IsValid(DroneFlightNavigationComponent))
	{
		FVector OutSafeTarget = TargetToUse;
		if (DroneFlightNavigationComponent->FindSafeFlightTarget(TargetToUse, OutSafeTarget))
		{
			SafeFlightTarget = OutSafeTarget;
		}
		else
		{
			SafeFlightTarget = TargetToUse;
		}
	}
	else
	{
		SafeFlightTarget = TargetToUse;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Direction = (SafeFlightTarget - CurrentLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FVector DesiredVelocity = Direction * AutopilotSpeed;

	CurrentAutopilotVelocity = FMath::VInterpTo(
		CurrentAutopilotVelocity,
		DesiredVelocity,
		DeltaTime,
		AutopilotAccelerationInterpSpeed
	);

	AddActorWorldOffset(CurrentAutopilotVelocity * DeltaTime, true);

	const FRotator TargetRotation = CurrentAutopilotVelocity.Rotation();
	const FRotator NewRotation = FMath::RInterpTo(
		GetActorRotation(),
		TargetRotation,
		DeltaTime,
		AutopilotRotationInterpSpeed
	);

	SetActorRotation(NewRotation);

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float DistanceToBot = FVector::Dist(GetActorLocation(), TargetBotActor->GetActorLocation());

	const bool bCanAttack = CurrentTime - LastAttackTime >= AttackCooldown;
	const bool bHasLineOfSight = IsValid(DronePerceptionComponent)
		? DronePerceptionComponent->HasLineOfSightToTarget()
		: true;

	if (DistanceToBot <= AttackRange && bHasLineOfSight && bCanAttack)
	{
		LastAttackTime = CurrentTime;

		ADSimCharacter* DSimCharacter = Cast<ADSimCharacter>(TargetBotActor);
		if (IsValid(DSimCharacter))
		{
			DSimCharacter->GetOverlappedDamage(100.0f);
			DroneExplode();
		}
	}
}

void ADSimDronePawn::OnDroneOverlapped(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	ADSimCharacter* DSimCharacter = Cast<ADSimCharacter>(OtherActor);
	if (IsValid(DSimCharacter))
	{
		DSimCharacter->GetOverlappedDamage(100.0f);
		DroneExplode();
	}

	if (IsValid(OtherActor) && OtherActor->ActorHasTag("Environment"))
	{
		DroneExplode();
	}
}

void ADSimDronePawn::DroneExplode()
{
	if (!ExplodeSphereComponent)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	ExplodeSphereComponent->GetOverlappingActors(OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		if (IsValid(Actor) && Actor->Implements<UDroneTargetInterface>())
		{
			Cast<IDroneTargetInterface>(Actor)->GetOverlappedDamage(100.0f);
		}
	}

	ADSimGameMode* GameMode = Cast<ADSimGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (IsValid(GameMode))
	{
		GameMode->PlayerEndGame();
	}
}

void ADSimDronePawn::StopLogic()
{
	GetWorldTimerManager().ClearTimer(LocationUpdateTimer);

	SetAutopilotState(EDroneAutopilotState::Stopped);

	
	if (IsValid(StateTreeComponent))
	{
		StateTreeComponent->StopLogic(TEXT("Game stopped"));
	}

	ADSimDroneController* DroneController = Cast<ADSimDroneController>(GetController());

	FInputModeUIOnly InputMode;
	if (IsValid(DroneController))
	{
		DroneController->HandleEndPlay();
		DroneController->SetInputMode(InputMode);
	}

	ACameraActor* CameraActor = Cast<ACameraActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ACameraActor::StaticClass()));

	if (IsValid(CameraActor) && IsValid(DroneController))
	{
		DroneController->SetViewTarget(CameraActor);
	}
}

void ADSimDronePawn::DrawLocations()
{
	UDSimBlueprintFunctionLibrary::DrawMovementPathInTheLevel(GetWorld(), Locations, FColor::Red, 120.0f, 20);
}

void ADSimDronePawn::OnDroneHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	UE_LOG(LogTemp, Warning, TEXT("Drone hit: %s"), *GetNameSafe(OtherActor));

	if (IsValid(OtherActor) && OtherActor->ActorHasTag("Environment"))
	{
		DroneExplode();
	}
}

void ADSimDronePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DroneMappingContext)
			{
				Subsystem->AddMappingContext(DroneMappingContext, 0);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Throttle)
		{
			EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &ADSimDronePawn::HandleThrottle);
			EnhancedInput->BindAction(IA_Throttle, ETriggerEvent::Completed, this, &ADSimDronePawn::ResetThrottle);
		}

		if (IA_Yaw)
		{
			EnhancedInput->BindAction(IA_Yaw, ETriggerEvent::Triggered, this, &ADSimDronePawn::HandleYaw);
			EnhancedInput->BindAction(IA_Yaw, ETriggerEvent::Completed, this, &ADSimDronePawn::ResetYaw);
		}

		if (IA_Pitch)
		{
			EnhancedInput->BindAction(IA_Pitch, ETriggerEvent::Triggered, this, &ADSimDronePawn::HandlePitch);
			EnhancedInput->BindAction(IA_Pitch, ETriggerEvent::Completed, this, &ADSimDronePawn::ResetPitch);
		}

		if (IA_Roll)
		{
			EnhancedInput->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &ADSimDronePawn::HandleRoll);
			EnhancedInput->BindAction(IA_Roll, ETriggerEvent::Completed, this, &ADSimDronePawn::ResetRoll);
		}
	}
}

void ADSimDronePawn::HandleThrottle(const FInputActionValue& Value)
{
	ThrottleInput = Value.Get<float>();
}

void ADSimDronePawn::HandleYaw(const FInputActionValue& Value)
{
	YawInput = Value.Get<float>();
}

void ADSimDronePawn::HandlePitch(const FInputActionValue& Value)
{
	PitchInput = Value.Get<float>();
}

void ADSimDronePawn::HandleRoll(const FInputActionValue& Value)
{
	RollInput = Value.Get<float>();
}

void ADSimDronePawn::ResetPitch(const FInputActionValue&)
{
	PitchInput = 0.f;
}

void ADSimDronePawn::ResetRoll(const FInputActionValue&)
{
	RollInput = 0.f;
}

void ADSimDronePawn::ResetYaw(const FInputActionValue&)
{
	YawInput = 0.f;
}

void ADSimDronePawn::ResetThrottle(const FInputActionValue&)
{
	ThrottleInput = 0.f;
}

void ADSimDronePawn::HandleMovementFromInput(float DeltaTime)
{
	const FVector UpMovement = FVector::UpVector * ThrottleInput * MoveSpeed * DeltaTime;
	AddActorWorldOffset(UpMovement, true);

	if (!FMath::IsNearlyZero(PitchInput))
	{
		const FVector ForwardMovement = GetActorForwardVector() * PitchInput * MoveSpeed * DeltaTime;
		AddActorWorldOffset(ForwardMovement, true);
	}

	if (!FMath::IsNearlyZero(RollInput))
	{
		const FVector RightMovement = GetActorRightVector() * RollInput * MoveSpeed * 0.5f * DeltaTime;
		AddActorWorldOffset(RightMovement, true);
	}

	const float YawDelta = YawInput * RotationSpeed * DeltaTime;
	FRotator RotationDelta = FRotator::ZeroRotator;
	RotationDelta.Yaw = YawDelta;

	AddActorLocalRotation(RotationDelta);

	const FRotator TargetTilt = FRotator(-PitchInput * MaxPitchAngle, 0.f, RollInput * MaxRollAngle);
	MeshComponent->SetRelativeRotation(
		FMath::RInterpTo(MeshComponent->GetRelativeRotation(), TargetTilt, DeltaTime, 5.f)
	);
}

void ADSimDronePawn::MoveAutopilotTowardsSafeTarget(float DeltaTime)
{
	if (!IsAutopilotEnabled())
	{
		return;
	}

	FVector TargetToUse = SafeFlightTarget;

	if (TargetToUse.IsNearlyZero())
	{
		TargetToUse = DesiredFlightTarget;
	}

	if (TargetToUse.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Direction = (TargetToUse - CurrentLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FVector DesiredVelocity = Direction * AutopilotSpeed;

	CurrentAutopilotVelocity = FMath::VInterpTo(
		CurrentAutopilotVelocity,
		DesiredVelocity,
		DeltaTime,
		AutopilotAccelerationInterpSpeed
	);

	AddActorWorldOffset(CurrentAutopilotVelocity * DeltaTime, true);

	const FRotator TargetRotation = CurrentAutopilotVelocity.Rotation();

	const FRotator NewRotation = FMath::RInterpTo(
		GetActorRotation(),
		TargetRotation,
		DeltaTime,
		AutopilotRotationInterpSpeed
	);

	SetActorRotation(NewRotation);
}

bool ADSimDronePawn::TryPrepareAttackDive()
{
	if (bAttackDiveActive)
	{
		return true;
	}
	
	if (!HasValidTargetBot() || !GetWorld())
	{
		return false;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastAttackTime < AttackCooldown)
	{
		return false;
	}

	const float DistanceToBot = FVector::Dist(GetActorLocation(), TargetBotActor->GetActorLocation());

	if (DistanceToBot > AttackDiveStartRange)
	{
		return false;
	}

	bool bHasLineOfSight = true;

	if (IsValid(DronePerceptionComponent))
	{
		bHasLineOfSight = DronePerceptionComponent->HasLineOfSightToTarget();
	}

	if (!bHasLineOfSight)
	{
		return false;
	}

	if (!IsValid(DroneFlightNavigationComponent))
	{
		return false;
	}

	FVector NewAttackTarget = FVector::ZeroVector;
	FHitResult GroundHit;

	const bool bFoundAttackTarget = DroneFlightNavigationComponent->FindAttackDiveTarget(
		TargetBotActor,
		NewAttackTarget,
		GroundHit
	);

	if (!bFoundAttackTarget)
	{
		return false;
	}

	FHitResult BlockingHit;

	const bool bPathClear = DroneFlightNavigationComponent->IsAttackDivePathClear(
		NewAttackTarget,
		TargetBotActor,
		BlockingHit
	);

	if (!bPathClear)
	{
		// Наприклад, дерево між дроном і ботом — не починаємо піке.
		return false;
	}

	AttackDiveTarget = NewAttackTarget;
	bAttackDiveActive = true;
	LastAttackTime = CurrentTime;

	SetDesiredFlightTarget(AttackDiveTarget);
	SetSafeFlightTarget(AttackDiveTarget);
	SetAutopilotState(EDroneAutopilotState::AttackRun);

	return true;
}

EDroneAttackDiveResult ADSimDronePawn::TickAttackDive(float DeltaTime)
{
	if (!bAttackDiveActive)
	{
		return EDroneAttackDiveResult::NotStarted;
	}

	if (!HasValidTargetBot())
	{
		bAttackDiveActive = false;
		return EDroneAttackDiveResult::Failed;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float DistanceToImpact = FVector::Dist(CurrentLocation, AttackDiveTarget);
	const float DistanceToBot = FVector::Dist(CurrentLocation, TargetBotActor->GetActorLocation());

	if (DistanceToImpact <= AttackImpactRadius || DistanceToBot <= AttackImpactRadius)
	{
		ADSimCharacter* DSimCharacter = Cast<ADSimCharacter>(TargetBotActor);
		if (IsValid(DSimCharacter))
		{
			DSimCharacter->GetOverlappedDamage(100.0f);
		}

		bAttackDiveActive = false;
		DroneExplode();

		return EDroneAttackDiveResult::Impact;
	}

	const FVector Direction = (AttackDiveTarget - CurrentLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		bAttackDiveActive = false;
		return EDroneAttackDiveResult::Failed;
	}

	const FVector DesiredVelocity = Direction * AttackDiveSpeed;

	CurrentAutopilotVelocity = FMath::VInterpTo(
		CurrentAutopilotVelocity,
		DesiredVelocity,
		DeltaTime,
		AttackDiveAccelerationInterpSpeed
	);

	const FVector MoveDelta = CurrentAutopilotVelocity * DeltaTime;
	const FVector NewLocation = CurrentLocation + MoveDelta;

	UE_LOG(LogTemp, Warning,
		TEXT("Dive: Current=%s Target=%s Dir=%s Vel=%s Delta=%s DistImpact=%.2f DT=%.4f"),
		*CurrentLocation.ToString(),
		*AttackDiveTarget.ToString(),
		*Direction.ToString(),
		*CurrentAutopilotVelocity.ToString(),
		*MoveDelta.ToString(),
		DistanceToImpact,
		DeltaTime
	);

	FHitResult SweepHit;

	SetActorLocation(
		NewLocation,
		true,
		&SweepHit,
		ETeleportType::None
	);

	if (SweepHit.bBlockingHit)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Dive blocked by: %s at %s"),
			*GetNameSafe(SweepHit.GetActor()),
			*SweepHit.ImpactPoint.ToString()
		);

		AActor* HitActor = SweepHit.GetActor();

		if (IsValid(HitActor) && HitActor == TargetBotActor)
		{
			ADSimCharacter* DSimCharacter = Cast<ADSimCharacter>(TargetBotActor);
			if (IsValid(DSimCharacter))
			{
				DSimCharacter->GetOverlappedDamage(100.0f);
			}

			bAttackDiveActive = false;
			DroneExplode();

			return EDroneAttackDiveResult::Impact;
		}

		const float HitToImpactDistance = FVector::Dist(SweepHit.ImpactPoint, AttackDiveTarget);

		if (HitToImpactDistance <= AttackImpactRadius * 1.5f)
		{
			bAttackDiveActive = false;
			DroneExplode();

			return EDroneAttackDiveResult::Impact;
		}

		bAttackDiveActive = false;
		DroneExplode();

		return EDroneAttackDiveResult::Blocked;
	}

	const FRotator TargetRotation = CurrentAutopilotVelocity.Rotation();

	const FRotator NewRotation = FMath::RInterpTo(
		GetActorRotation(),
		TargetRotation,
		DeltaTime,
		AttackDiveRotationInterpSpeed
	);

	SetActorRotation(NewRotation);

	return EDroneAttackDiveResult::InProgress;
}

void ADSimDronePawn::AbortAttackDive()
{
	bAttackDiveActive = false;
	AttackDiveTarget = FVector::ZeroVector;
	CurrentAutopilotVelocity = FVector::ZeroVector;

	if (AutopilotState == EDroneAutopilotState::AttackRun)
	{
		SetAutopilotState(EDroneAutopilotState::Pursuit);
	}
}

void ADSimDronePawn::ResetAutopilotRuntime()
{
	AbortAttackDive();

	DesiredFlightTarget = FVector::ZeroVector;
	SafeFlightTarget = FVector::ZeroVector;
	CurrentAutopilotVelocity = FVector::ZeroVector;
	LastAttackTime = -1000.f;

	SetAutopilotState(EDroneAutopilotState::Idle);
}

void ADSimDronePawn::StartAutopilotLogic()
{
	if (IsValid(StateTreeComponent) && DroneControlMode != EDroneControlMode::HumanControlled)
	{
		StateTreeComponent->StartLogic();
	}
}

void ADSimDronePawn::StopAutopilotLogic()
{
	SetAutopilotState(EDroneAutopilotState::Stopped);
	AbortAttackDive();

	CurrentAutopilotVelocity = FVector::ZeroVector;

	if (IsValid(StateTreeComponent))
	{
		StateTreeComponent->StopLogic(TEXT("Simulation episode stopped"));
	}
}