#include "Customization/IntPointPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Layout/Children.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"
#include "ValueLadderLog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"

namespace
{
	const FName VLT_NAME_X(TEXT("X"));
	const FName VLT_NAME_Y(TEXT("Y"));
	const FName MetadataNames[] = {
		TEXT("UIMin"),
		TEXT("UIMax"),
		TEXT("SliderExponent"),
		TEXT("Delta"),
		TEXT("LinearDeltaSensitivity"),
		TEXT("ShiftMultiplier"),
		TEXT("CtrlMultiplier"),
		TEXT("SupportDynamicSliderMaxValue"),
		TEXT("SupportDynamicSliderMinValue"),
		TEXT("ClampMin"),
		TEXT("ClampMax")
	};

	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		switch (NumericType)
		{
		case EValueLadderNumericType::Float:
			return TEXT("Float");
		case EValueLadderNumericType::Double:
			return TEXT("Double");
		case EValueLadderNumericType::Int32:
			return TEXT("Int32");
		default:
			return TEXT("Unknown");
		}
	}
}

FIntPointPropertyCustomization::~FIntPointPropertyCustomization()
{
	for (const FValueLadderTargetHandle Handle : RegisteredHandles)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][IntPoint] Unregister handle=%llu"), static_cast<uint64>(Handle));
		FValueLadderTargetRegistry::Get().UnregisterTarget(Handle);
	}
	RegisteredHandles.Reset();
}

TSharedRef<IPropertyTypeCustomization> FIntPointPropertyCustomization::MakeInstance()
{
	return MakeShared<FIntPointPropertyCustomization>();
}

void FIntPointPropertyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> XHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_X);
	const TSharedPtr<IPropertyHandle> YHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_Y);
	TSharedRef<SWidget> NameWidget = PropertyHandle->CreatePropertyNameWidget();
	auto ApplyFallbackHeader = [&HeaderRow, &NameWidget, &PropertyHandle]()
	{
		HeaderRow
		.NameContent()
		[
			NameWidget
		]
		.ValueContent()
		.MinDesiredWidth(140.0f)
		.MaxDesiredWidth(600.0f)
		[
			PropertyHandle->CreatePropertyValueWidget()
		];
	};

	for (const FValueLadderTargetHandle Handle : RegisteredHandles)
	{
		FValueLadderTargetRegistry::Get().UnregisterTarget(Handle);
	}
	RegisteredHandles.Reset();

	if (!XHandle.IsValid() || !YHandle.IsValid() || !XHandle->IsValidHandle() || !YHandle->IsValidHandle())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][IntPoint] Missing X/Y child handles for property '%s'; header widgets will remain unregistered."), *PropertyHandle->GetPropertyDisplayName().ToString());
		ApplyFallbackHeader();
		return;
	}

	EValueLadderNumericType NumericType = EValueLadderNumericType::Int32;
	if (!ResolveNumericType(XHandle.ToSharedRef(), NumericType) || !ResolveNumericType(YHandle.ToSharedRef(), NumericType))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][IntPoint] Failed to resolve integer numeric type for property '%s'; header widgets will remain unregistered."), *PropertyHandle->GetPropertyDisplayName().ToString());
		ApplyFallbackHeader();
		return;
	}

	for (const FName& MetadataName : MetadataNames)
	{
		const FString MetadataValue = PropertyHandle->GetMetaData(MetadataName);
		if (!MetadataValue.IsEmpty())
		{
			XHandle->SetInstanceMetaData(MetadataName, MetadataValue);
			YHandle->SetInstanceMetaData(MetadataName, MetadataValue);
		}
	}

	TSharedRef<SWidget> XValueWidget = XHandle->CreatePropertyValueWidget();
	TSharedRef<SWidget> YValueWidget = YHandle->CreatePropertyValueWidget();

	TArray<FValueLadderPropertyTarget> Targets;
	for (const TSharedPtr<IPropertyHandle>& LeafHandle : {XHandle, YHandle})
	{
		FValueLadderPropertyTarget Target;
		Target.PropertyHandle = LeafHandle;
		Target.NumericType = NumericType;
		Target.SemanticRole = EValueLadderSemanticRole::IntegerDiscrete;
		Target.ComponentName = LeafHandle->GetProperty() != nullptr ? LeafHandle->GetProperty()->GetFName() : NAME_None;
		Targets.Add(Target);
	}

	RegisterWidgetSubtree(XValueWidget, Targets[0]);
	RegisterWidgetSubtree(YValueWidget, Targets[1]);

	TSharedRef<SHorizontalBox> ValueWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 2.0f, 3.0f, 2.0f)
		[
			XValueWidget
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 2.0f, 0.0f, 2.0f)
		[
			YValueWidget
		];

	HeaderRow
	.NameContent()
	[
		NameWidget
	]
	.ValueContent()
	.MinDesiredWidth(140.0f)
	.MaxDesiredWidth(600.0f)
	[
		ValueWidget
	];

	UE_LOG(LogValueLadder, Display, TEXT("[Customization][IntPoint] Registered property '%s' header targets=%d type=%s"), *PropertyHandle->GetPropertyDisplayName().ToString(), Targets.Num(), ToNumericTypeString(NumericType));
}

void FIntPointPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> XHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_X);
	const TSharedPtr<IPropertyHandle> YHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_Y);

	if (XHandle.IsValid() && XHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(XHandle.ToSharedRef());
	}

	if (YHandle.IsValid() && YHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(YHandle.ToSharedRef());
	}
	}

TSharedPtr<IPropertyHandle> FIntPointPropertyCustomization::FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex);
		if (!ChildHandle.IsValid() || !ChildHandle->IsValidHandle())
		{
			continue;
		}

		const FProperty* ChildProperty = ChildHandle->GetProperty();
		if (ChildProperty != nullptr && ChildProperty->GetFName() == PropertyName)
		{
			return ChildHandle;
		}
	}

	return nullptr;
}

bool FIntPointPropertyCustomization::ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType)
{
	const FProperty* Property = PropertyHandle->GetProperty();
	if (Property == nullptr)
	{
		return false;
	}

	if (Property->IsA<FIntProperty>())
	{
		OutType = EValueLadderNumericType::Int32;
		return true;
	}

	return false;
}

void FIntPointPropertyCustomization::RegisterWidgetSubtree(const TSharedRef<SWidget>& RootWidget, const FValueLadderPropertyTarget& Target)
{
	RegisteredHandles.Add(FValueLadderTargetRegistry::Get().RegisterTarget(RootWidget, Target));

	FChildren* Children = RootWidget->GetAllChildren();
	if (Children == nullptr)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		RegisterWidgetSubtree(Children->GetChildAt(ChildIndex), Target);
	}
}
