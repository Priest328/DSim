// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/Actors/DSimCoverActor.h"

// Sets default values
ADSimCoverActor::ADSimCoverActor()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(MeshComp);
}

// Called when the game starts or when spawned
void ADSimCoverActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADSimCoverActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

