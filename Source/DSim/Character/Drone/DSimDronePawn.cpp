// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/Character/Drone/DSimDronePawn.h"

#include <dshow.h>

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DSim/Actors/SphereActor.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Game/DSimGameMode.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "DSim/Player/DSimDroneController.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"


ADSimDronePawn::ADSimDronePawn()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("CollisionComp");
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	FloatingComponent = CreateDefaultSubobject<UFloatingPawnMovement>("MovementComp");
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArmComponent");
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComp");
	ExplodeSphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComp");

	SetRootComponent(BoxComponent);

	MeshComponent->SetupAttachment(GetRootComponent());
	SpringArmComponent->SetupAttachment(MeshComponent);
	CameraComponent->SetupAttachment(SpringArmComponent);

	ExplodeSphereComponent->SetupAttachment(GetRootComponent());
	ExplodeSphereComponent->SetSphereRadius(1000.0f);

	BoxComponent->SetSimulatePhysics(true);
	BoxComponent->SetEnableGravity(false);

	PrimaryActorTick.bCanEverTick = true;
}

void ADSimDronePawn::BeginPlay()
{
	Super::BeginPlay();

	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &ADSimDronePawn::OnDroneOverlapped);

	ADSimGameMode* GameMode = Cast<ADSimGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (IsValid(GameMode))
	{
		GameMode->OnStopAllCharacters.AddDynamic(this, &ADSimDronePawn::StopLogic);
		GameMode->OnNeedToDrawDebugSpheres.AddDynamic(this, &ADSimDronePawn::DrawLocations);
	}

	GetWorldTimerManager().SetTimer(LocationUpdateTimer, [this]()
	{
		Locations.Add(GetActorLocation());
		FVector AdjustedPoint = FVector(
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z
	);
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

	HandleMovementFromInput(DeltaTime);
}

void ADSimDronePawn::OnDroneOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                       UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                       const FHitResult& SweepResult)
{
	ADSimCharacter* DSimCharacter = Cast<ADSimCharacter>(OtherActor);
	if (IsValid(DSimCharacter))
	{
		DSimCharacter->GetOverlappedDamage(100.0f);
		DroneExplode();
	}

	if (OtherActor->ActorHasTag("Environment"))
	{
		DroneExplode();
	}
}

void ADSimDronePawn::DroneExplode()
{
	if (!ExplodeSphereComponent) return;

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

	ADSimDroneController* DroneController = Cast<ADSimDroneController>(GetController());

	FInputModeUIOnly InputMode;
	ShowCursor(true);
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

void ADSimDronePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
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
	// Vertical (Throttle)
	FVector UpMovement = FVector::UpVector * ThrottleInput * MoveSpeed * DeltaTime;
	AddActorWorldOffset(UpMovement, true);

	// Forward movement based on pitch
	if (!FMath::IsNearlyZero(PitchInput))
	{
		FVector Forward = GetActorForwardVector();
		FVector ForwardMovement = Forward * PitchInput * MoveSpeed * DeltaTime;
		AddActorWorldOffset(ForwardMovement, true);
	}

	// Side movement based on roll (optional if you want strafe-like behavior)
	if (!FMath::IsNearlyZero(RollInput))
	{
		FVector Right = GetActorRightVector();
		FVector RightMovement = Right * RollInput * MoveSpeed * 0.5f * DeltaTime;
		AddActorWorldOffset(RightMovement, true);
	}

	// Apply rotation
	float YawDelta = YawInput * RotationSpeed * DeltaTime;
	FRotator RotationDelta = FRotator::ZeroRotator;
	RotationDelta.Yaw = YawDelta;

	AddActorLocalRotation(RotationDelta);

	// Simulate tilt (visual only)
	FRotator TargetTilt = FRotator(-PitchInput * MaxPitchAngle, 0.f, RollInput * MaxRollAngle);
	MeshComponent->SetRelativeRotation(FMath::RInterpTo(MeshComponent->GetRelativeRotation(), TargetTilt, DeltaTime,
	                                                    5.f));
}
