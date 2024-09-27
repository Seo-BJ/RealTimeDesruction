// Copyright Epic Games, Inc. All Rights Reserved.

#include "RealTimeDesructionGameMode.h"
#include "RealTimeDesructionCharacter.h"
#include "UObject/ConstructorHelpers.h"

ARealTimeDesructionGameMode::ARealTimeDesructionGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
