// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DSimDronePawn.generated.h"

class ASphereActor;
class USphereComponent;
class USpringArmComponent;
class UBoxComponent;
class UCameraComponent;
class UFloatingPawnMovement;
struct FInputActionValue;
class UInputMappingContext;
class UInputAction;

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
	void OnDroneOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

private:
	// Input handlers
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

public:
	UPROPERTY(EditAnywhere, Category = "Drone|Control")
	bool bSmoothStabilization = true;

	UPROPERTY(EditAnywhere, Category = "Drone|Control")
	float InputReturnSpeed = 3.f; // Interpolation speed

	UPROPERTY(EditAnywhere)
	TSubclassOf<ASphereActor> SphereMarkerClass;
	
public:	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DroneMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Throttle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Yaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Roll;

protected:
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<UBoxComponent> BoxComponent;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<UFloatingPawnMovement> FloatingComponent;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Component")
	TObjectPtr<USphereComponent> ExplodeSphereComponent;
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

	TArray<FVector> Locations;

	//TODO: Delete later. Only for debug purposes 
	FTimerHandle LocationUpdateTimer;
};


