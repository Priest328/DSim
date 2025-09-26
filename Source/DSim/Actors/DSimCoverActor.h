// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DSimCoverActor.generated.h"

UCLASS()
class DSIM_API ADSimCoverActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ADSimCoverActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> MeshComp;
};
