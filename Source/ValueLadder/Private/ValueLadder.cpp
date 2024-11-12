// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValueLadder.h"

#include "ValueLadderInputPreProcessor.h"

#define LOCTEXT_NAMESPACE "FValueLadderModule"

void FValueLadderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	//Register InputPreProcessor hacker
	if(FSlateApplication::IsInitialized())
	{
		LadderInputPreProcessor = MakeShareable(new FValueLadderInputPreProcessor());
		bool bPreProcessor = FSlateApplication::Get().RegisterInputPreProcessor(LadderInputPreProcessor);		
	}
}

void FValueLadderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FValueLadderModule, ValueLadder)