#include "Customization/TransformPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Layout/Children.h"
#include "PropertyHandle.h"
#include "ValueLadderLog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"

namespace
{
	const FName VLT_NAME_Rotation(TEXT("Rotation"));
	const FName VLT_NAME_Translation(TEXT("Translation"));
	const FName VLT_NAME_Scale3D(TEXT("Scale3D"));
	const FName VLT_NAME_X(TEXT("X"));
	const FName VLT_NAME_Y(TEXT("Y"));
	const FName VLT_NAME_Z(TEXT("Z"));
	const FName VLT_NAME_Roll(TEXT("Roll"));
	const FName VLT_NAME_Pitch(TEXT("Pitch"));
	const FName VLT_NAME_Yaw(TEXT("Yaw"));

	EValueLadderSemanticRole GetSemanticRoleFromTransformField(const FValueLadderPropertyTarget::ETransformField TransformField)
	{
		switch (TransformField)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return EValueLadderSemanticRole::Translation;
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return EValueLadderSemanticRole::Rotation;
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return EValueLadderSemanticRole::Scale;
		default:
			return EValueLadderSemanticRole::GenericScalar;
		}
	}

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

FTransformPropertyCustomization::~FTransformPropertyCustomization()
{
	for (const FValueLadderTargetHandle Handle : RegisteredHandles)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Transform] Unregister handle=%llu"), static_cast<uint64>(Handle));
		FValueLadderTargetRegistry::Get().UnregisterTarget(Handle);
	}
	RegisteredHandles.Reset();
}

TSharedRef<IPropertyTypeCustomization> FTransformPropertyCustomization::MakeInstance()
{
	return MakeShared<FTransformPropertyCustomization>();
}

void FTransformPropertyCustomization::CustomizeHeader(
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
	.MinDesiredWidth(0.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNullWidget::NullWidget
	];
}

void FTransformPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> TranslationHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_Translation);
	const TSharedPtr<IPropertyHandle> RotationHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_Rotation);
	const TSharedPtr<IPropertyHandle> ScaleHandle = FindChildHandleByPropertyName(PropertyHandle, VLT_NAME_Scale3D);

	if (!TranslationHandle.IsValid() || !RotationHandle.IsValid() || !ScaleHandle.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][Transform] Missing one or more expected Transform child handles for property '%s'."), *PropertyHandle->GetPropertyDisplayName().ToString());
		return;
	}

	const TSharedPtr<IPropertyHandle> TranslationXHandle = FindChildHandleByPropertyName(TranslationHandle.ToSharedRef(), VLT_NAME_X);
	EValueLadderNumericType NumericType = EValueLadderNumericType::Double;
	if (!TranslationXHandle.IsValid() || !ResolveNumericType(TranslationXHandle.ToSharedRef(), NumericType))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][Transform] Failed to resolve numeric type for property '%s'."), *PropertyHandle->GetPropertyDisplayName().ToString());
		return;
	}

	AddDirectVectorRow(ChildBuilder, TranslationHandle.ToSharedRef(), NSLOCTEXT("ValueLadder", "TransformLocation", "Location"), FValueLadderPropertyTarget::ETransformField::Location, NumericType, TEXT("Location"));
	AddRotationProxyRow(ChildBuilder, PropertyHandle, RotationHandle.ToSharedRef(), NumericType);
	AddDirectVectorRow(ChildBuilder, ScaleHandle.ToSharedRef(), NSLOCTEXT("ValueLadder", "TransformScale", "Scale"), FValueLadderPropertyTarget::ETransformField::Scale, NumericType, TEXT("Scale"));
}

bool FTransformPropertyCustomization::ResolveNumericType(const TSharedRef<IPropertyHandle>& PropertyHandle, EValueLadderNumericType& OutType)
{
	const FProperty* Property = PropertyHandle->GetProperty();
	if (Property == nullptr)
	{
		return false;
	}

	if (Property->IsA<FFloatProperty>())
	{
		OutType = EValueLadderNumericType::Float;
		return true;
	}

	if (Property->IsA<FDoubleProperty>())
	{
		OutType = EValueLadderNumericType::Double;
		return true;
	}

	return false;
}

TSharedPtr<IPropertyHandle> FTransformPropertyCustomization::FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName)
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

void FTransformPropertyCustomization::CollectWidgetsByTypePrefix(const TSharedRef<SWidget>& RootWidget, const FString& TypePrefix, TArray<TSharedRef<SWidget>>& OutWidgets)
{
	if (RootWidget->GetTypeAsString().StartsWith(TypePrefix))
	{
		OutWidgets.Add(RootWidget);
	}

	FChildren* Children = RootWidget->GetAllChildren();
	if (Children == nullptr)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		CollectWidgetsByTypePrefix(Children->GetChildAt(ChildIndex), TypePrefix, OutWidgets);
	}
}

void FTransformPropertyCustomization::RegisterWidgetSubtree(const TSharedRef<SWidget>& RootWidget, const FValueLadderPropertyTarget& Target)
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

void FTransformPropertyCustomization::RegisterNumericEntrySubtrees(const TSharedRef<SWidget>& RootWidget, const TArray<FValueLadderPropertyTarget>& Targets, const TCHAR* ContextLabel)
{
	TArray<TSharedRef<SWidget>> NumericEntryWidgets;
	CollectWidgetsByTypePrefix(RootWidget, TEXT("SNumericEntryBox<"), NumericEntryWidgets);

	if (NumericEntryWidgets.Num() != Targets.Num())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][Transform] %s expected %d numeric entry widgets but found %d under root type=%s."), ContextLabel, Targets.Num(), NumericEntryWidgets.Num(), *RootWidget->GetTypeAsString());
	}

	const int32 WidgetCount = FMath::Min(NumericEntryWidgets.Num(), Targets.Num());
	for (int32 WidgetIndex = 0; WidgetIndex < WidgetCount; ++WidgetIndex)
	{
		RegisterWidgetSubtree(NumericEntryWidgets[WidgetIndex], Targets[WidgetIndex]);
		UE_LOG(LogValueLadder, Display, TEXT("[Customization][Transform] Registered %s widget subtree index=%d root=%p rootType=%s component=%s kind=%d"), ContextLabel, WidgetIndex, static_cast<const void*>(&NumericEntryWidgets[WidgetIndex].Get()), *NumericEntryWidgets[WidgetIndex]->GetTypeAsString(), *Targets[WidgetIndex].ComponentName.ToString(), static_cast<int32>(Targets[WidgetIndex].Kind));
	}
}

void FTransformPropertyCustomization::AddDirectVectorRow(
	IDetailChildrenBuilder& ChildBuilder,
	const TSharedRef<IPropertyHandle>& GroupHandle,
	const FText& DisplayName,
	FValueLadderPropertyTarget::ETransformField TransformField,
	EValueLadderNumericType NumericType,
	const TCHAR* ContextLabel)
{
	const TSharedPtr<IPropertyHandle> XHandle = FindChildHandleByPropertyName(GroupHandle, VLT_NAME_X);
	const TSharedPtr<IPropertyHandle> YHandle = FindChildHandleByPropertyName(GroupHandle, VLT_NAME_Y);
	const TSharedPtr<IPropertyHandle> ZHandle = FindChildHandleByPropertyName(GroupHandle, VLT_NAME_Z);
	if (!XHandle.IsValid() || !YHandle.IsValid() || !ZHandle.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Customization][Transform] %s row is missing one or more component handles."), ContextLabel);
		return;
	}

	TSharedRef<SWidget> NameWidget = GroupHandle->CreatePropertyNameWidget(DisplayName);
	TSharedRef<SWidget> ValueWidget = GroupHandle->CreatePropertyValueWidget();

	FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(DisplayName);
	Row
	.NameContent()
	[
		NameWidget
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		ValueWidget
	];

	TArray<FValueLadderPropertyTarget> Targets;
	for (const TSharedPtr<IPropertyHandle>& LeafHandle : {XHandle, YHandle, ZHandle})
	{
		FValueLadderPropertyTarget Target;
		Target.PropertyHandle = LeafHandle;
		Target.NumericType = NumericType;
		Target.SemanticRole = GetSemanticRoleFromTransformField(TransformField);
		Target.bIsVectorComponent = true;
		Target.TransformField = TransformField;
		Target.ComponentName = LeafHandle->GetProperty()->GetFName();
		Targets.Add(Target);
	}

	RegisterNumericEntrySubtrees(ValueWidget, Targets, ContextLabel);
}

void FTransformPropertyCustomization::AddRotationProxyRow(
	IDetailChildrenBuilder& ChildBuilder,
	const TSharedRef<IPropertyHandle>& TransformHandle,
	const TSharedRef<IPropertyHandle>& RotationHandle,
	EValueLadderNumericType NumericType)
{
	TSharedRef<SWidget> NameWidget = RotationHandle->CreatePropertyNameWidget(NSLOCTEXT("ValueLadder", "TransformRotation", "Rotation"));
	TSharedRef<SWidget> ValueWidget = RotationHandle->CreatePropertyValueWidget();

	FDetailWidgetRow& Row = ChildBuilder.AddCustomRow(NSLOCTEXT("ValueLadder", "TransformRotationRow", "Rotation"));
	Row
	.NameContent()
	[
		NameWidget
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		ValueWidget
	];

	TArray<FValueLadderPropertyTarget> Targets;
	for (const FName& ComponentName : {VLT_NAME_Roll, VLT_NAME_Pitch, VLT_NAME_Yaw})
	{
		FValueLadderPropertyTarget Target;
		Target.PropertyHandle = TransformHandle;
		Target.Kind = FValueLadderPropertyTarget::ETargetKind::TransformProxy;
		Target.NumericType = NumericType;
		Target.SemanticRole = EValueLadderSemanticRole::Rotation;
		Target.bIsVectorComponent = true;
		Target.TransformField = FValueLadderPropertyTarget::ETransformField::Rotation;
		Target.ComponentName = ComponentName;
		Targets.Add(Target);
	}

	RegisterNumericEntrySubtrees(ValueWidget, Targets, TEXT("Rotation"));
}
