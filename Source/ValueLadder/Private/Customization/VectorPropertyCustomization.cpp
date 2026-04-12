#include "Customization/VectorPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

FVectorPropertyCustomization::~FVectorPropertyCustomization()
{
	for (const FValueLadderTargetHandle Handle : RegisteredHandles)
	{
		FValueLadderTargetRegistry::Get().UnregisterTarget(Handle);
	}
	RegisteredHandles.Reset();
}

TSharedRef<IPropertyTypeCustomization> FVectorPropertyCustomization::MakeInstance()
{
	return MakeShared<FVectorPropertyCustomization>();
}

void FVectorPropertyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(140.0f)
	.MaxDesiredWidth(600.0f)
	[
		PropertyHandle->CreatePropertyValueWidget()
	];
}

void FVectorPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
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
		if (ChildProperty == nullptr ||
			(!ChildProperty->IsA<FFloatProperty>() && !ChildProperty->IsA<FDoubleProperty>()))
		{
			continue;
		}

		const EValueLadderNumericType NumericType = ChildProperty->IsA<FDoubleProperty>()
			? EValueLadderNumericType::Double
			: EValueLadderNumericType::Float;

		TSharedRef<SWidget> ValueWidget = ChildHandle->CreatePropertyValueWidget();
		FValueLadderPropertyTarget Target;
		Target.PropertyHandle = ChildHandle;
		Target.NumericType = NumericType;
		Target.bIsVectorComponent = true;
		RegisteredHandles.Add(FValueLadderTargetRegistry::Get().RegisterTarget(ValueWidget, Target));

		FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(ChildHandle->GetPropertyDisplayName());
		Row
		.NameContent()
		[
			ChildHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(140.0f)
		.MaxDesiredWidth(600.0f)
		[
			ValueWidget
		];
	}
}
