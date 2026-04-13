// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValueLadder.h"

#include "Input/ValueLadderInputProcessor.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"
#include "Customization/NumericPropertyCustomization.h"
#include "Customization/TransformPropertyCustomization.h"
#include "Customization/VectorPropertyCustomization.h"
#include "Input/ComponentTransformDetailsBridge.h"

#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FValueLadderModule"

void FValueLadderModule::StartupModule()
{
	UE_LOG(LogValueLadder, Display, TEXT("[Module] StartupModule begin. SlateInitialized=%s"), FSlateApplication::IsInitialized() ? TEXT("true") : TEXT("false"));

	if (FSlateApplication::IsInitialized())
	{
		LadderInputProcessor = MakeShared<FValueLadderInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(LadderInputProcessor);

		if (const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>())
		{
			UE_LOG(
				LogValueLadder,
				Display,
				TEXT("[Module] Input preprocessor registered. Trigger=%s RequireAlt=%s ShowOverlay=%s ThresholdPx=%.2f"),
				*Settings->TriggerMouseButton.ToString(),
				Settings->bRequireAltModifier ? TEXT("true") : TEXT("false"),
				Settings->bShowOverlay ? TEXT("true") : TEXT("false"),
				Settings->DragActivationThresholdPx);
		}
		else
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Module] Input preprocessor registered but settings default object was null."));
		}
	}
	else
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Module] SlateApplication not initialized during StartupModule; input preprocessor registration skipped."));
	}

	RegisterPropertyCustomizations();
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FComponentTransformDetailsBridge::Get().Register(PropertyModule);
	}
}

void FValueLadderModule::ShutdownModule()
{
	UE_LOG(LogValueLadder, Display, TEXT("[Module] ShutdownModule begin. SlateInitialized=%s ProcessorValid=%s"), FSlateApplication::IsInitialized() ? TEXT("true") : TEXT("false"), LadderInputProcessor.IsValid() ? TEXT("true") : TEXT("false"));

	if (FSlateApplication::IsInitialized() && LadderInputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(LadderInputProcessor.ToSharedRef());
		LadderInputProcessor.Reset();
		UE_LOG(LogValueLadder, Display, TEXT("[Module] Input preprocessor unregistered."));
	}

	UnregisterPropertyCustomizations();
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FComponentTransformDetailsBridge::Get().Unregister(PropertyModule);
	}
}

void FValueLadderModule::RegisterPropertyCustomizations()
{
	if (bPropertyCustomizationsRegistered)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Module] Property customizations already registered; skipping duplicate registration."));
		return;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Module] Registering property customizations for float/double/int32/int/vector/transform."));
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("FloatProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("DoubleProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("IntProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("float"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("double"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int32"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Vector"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVectorPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Transform"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTransformPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Transform3f"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTransformPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Transform3d"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTransformPropertyCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	bPropertyCustomizationsRegistered = true;
	UE_LOG(LogValueLadder, Display, TEXT("[Module] Property customizations registered and module change notified."));
}

void FValueLadderModule::UnregisterPropertyCustomizations()
{
	if (!bPropertyCustomizationsRegistered)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Module] Property customizations already unregistered; skipping."));
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Module] PropertyEditor module not loaded during unregister; clearing local registration flag only."));
		bPropertyCustomizationsRegistered = false;
		return;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Module] Unregistering property customizations."));
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("FloatProperty"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("DoubleProperty"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("IntProperty"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("float"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("double"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int32"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Vector"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Transform"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Transform3f"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Transform3d"));
	PropertyModule.NotifyCustomizationModuleChanged();

	bPropertyCustomizationsRegistered = false;
	UE_LOG(LogValueLadder, Display, TEXT("[Module] Property customizations unregistered."));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FValueLadderModule, ValueLadder)
