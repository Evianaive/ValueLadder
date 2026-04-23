// Copyright Epic Games, Inc. All Rights Reserved.

#include "ValueLadder.h"

#include "Input/ValueLadderInputProcessor.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"
#include "Customization/IntPointPropertyCustomization.h"
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
		StaticCastSharedPtr<FValueLadderInputProcessor>(LadderInputProcessor)->CancelActiveGesture();
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

	UE_LOG(LogValueLadder, Display, TEXT("[Module] Registering property customizations for scalar numerics, intpoint, vector, and transform."));
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("DetailCustomizations")))
	{
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("DetailCustomizations"));
		UE_LOG(LogValueLadder, Display, TEXT("[Module] Loaded DetailCustomizations before ValueLadder property overrides."));
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("FloatProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("DoubleProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ByteProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Int8Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Int16Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("IntProperty"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Int64Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("UInt16Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("UInt32Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("UInt64Property"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("float"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("double"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("uint8"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int8"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int16"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int32"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("int64"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("uint16"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("uint32"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("uint64"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNumericPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("IntPoint"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FIntPointPropertyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Int32Point"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FIntPointPropertyCustomization::MakeInstance));
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
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ByteProperty"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Int8Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Int16Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("IntProperty"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Int64Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("UInt16Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("UInt32Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("UInt64Property"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("float"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("double"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("uint8"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int8"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int16"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int32"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("int64"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("uint16"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("uint32"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("uint64"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("IntPoint"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Int32Point"));
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
