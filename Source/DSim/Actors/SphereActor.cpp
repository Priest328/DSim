// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/Actors/SphereActor.h"

#include "Components/SphereComponent.h"

// Sets default values
ASphereActor::ASphereActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SphereComponent = CreateDefaultSubobject<USphereComponent>("Sphere");
}

// Called when the game starts or when spawned
void ASphereActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASphereActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

