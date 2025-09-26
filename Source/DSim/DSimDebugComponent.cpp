// Fill out your copyright notice in the Description page of Project Settings.


#include "DSimDebugComponent.h"

UDSimDebugComponent::UDSimDebugComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UDSimDebugComponent::BeginPlay() {
	Super::BeginPlay();
}

void UDSimDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDSimDebugComponent::UpdateSelectedAction(EBotAction Action) {
	DebugInfo.LastSelectedAction = Action;
}

void UDSimDebugComponent::UpdateReward(float Reward) {
	DebugInfo.LastReward = Reward;
}

void UDSimDebugComponent::UpdateScores(float GoalScore, float CoverScore, float RandomScore) {
	DebugInfo.ActionScores.TowardGoalScore = GoalScore;
	DebugInfo.ActionScores.TowardCoverScore = CoverScore;
	DebugInfo.ActionScores.RandomMoveScore = RandomScore;
}

const FDebugData& UDSimDebugComponent::GetDebugData() const {
	return DebugInfo;
}
