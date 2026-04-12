// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValueLadder.h"

#include "Input/ValueLadderInputProcessor.h"
#include "Customization/NumericPropertyCustomization.h"
#include "Customization/VectorPropertyCustomization.h"

#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FValueLadderModule"

void FValueLadderModule::StartupModule()
{
	if (FSlateApplication::IsInitialized())
	{
		LadderInputProcessor = MakeShared<FValueLadderInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(LadderInputProcessor);
	}

	RegisterPropertyCustomizations();
}

void FValueLadderModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized() && LadderInputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(LadderInputProcessor.ToSharedRef());
		LadderInputProcessor.Reset();
	}

	UnregisterPropertyCustomizations();
}

void FValueLadderModule::RegisterPropertyCustomizations()
{
	if (bPropertyCustomizationsRegistered)
	{
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("float"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("double"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int32"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Vector"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVectorPropertyCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	bPropertyCustomizationsRegistered = true;
}

void FValueLadderModule::UnregisterPropertyCustomizations()
{
	if (!bPropertyCustomizationsRegistered)
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		bPropertyCustomizationsRegistered = false;
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("float"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("double"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int32"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Vector"));
	PropertyModule.NotifyCustomizationModuleChanged();

	bPropertyCustomizationsRegistered = false;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FValueLadderModule, ValueLadder)
