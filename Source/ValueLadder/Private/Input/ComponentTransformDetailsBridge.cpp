#include "Input/ComponentTransformDetailsBridge.h"

#include "Components/SceneComponent.h"
#include "IDetailsView.h"
#include "IDetailTreeNode.h"
#include "Layout/Children.h"
#include "Misc/AxisDisplayInfo.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Types/ISlateMetaData.h"
#include "ValueLadderLog.h"
#include "Widgets/SWidget.h"

namespace
{
	const FName VLT_NAME_Location(TEXT("Location"));
	const FName VLT_NAME_Rotation(TEXT("Rotation"));
	const FName VLT_NAME_Scale(TEXT("Scale"));
	const FName VLT_NAME_X(TEXT("X"));
	const FName VLT_NAME_Y(TEXT("Y"));
	const FName VLT_NAME_Z(TEXT("Z"));
	const FName VLT_NAME_Roll(TEXT("Roll"));
	const FName VLT_NAME_Pitch(TEXT("Pitch"));
	const FName VLT_NAME_Yaw(TEXT("Yaw"));

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

	const TCHAR* ToTransformFieldString(const FValueLadderPropertyTarget::ETransformField Field)
	{
		switch (Field)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return TEXT("Location");
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return TEXT("Rotation");
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return TEXT("Scale");
		default:
			return TEXT("Unknown");
		}
	}

bool TryMapRowTagToField(const FName& Tag, FValueLadderPropertyTarget::ETransformField& OutField)
	{
		if (Tag == VLT_NAME_Location || Tag == TEXT("DetailRowItem.Location"))
		{
			OutField = FValueLadderPropertyTarget::ETransformField::Location;
			return true;
		}

		if (Tag == VLT_NAME_Rotation || Tag == TEXT("DetailRowItem.Rotation"))
		{
			OutField = FValueLadderPropertyTarget::ETransformField::Rotation;
			return true;
		}

		if (Tag == VLT_NAME_Scale || Tag == TEXT("DetailRowItem.Scale"))
		{
			OutField = FValueLadderPropertyTarget::ETransformField::Scale;
			return true;
		}

		return false;
	}

	bool IsCachedFieldValid(const FComponentTransformDetailsBridge::FCachedTransformField& CachedField)
	{
		return CachedField.PropertyHandle.IsValid() && CachedField.PropertyHandle->IsValidHandle();
	}

	FComponentTransformDetailsBridge::FCachedTransformField* GetMutableCachedField(
		FComponentTransformDetailsBridge::FDetailsViewTransformCache& Cache,
		const FValueLadderPropertyTarget::ETransformField Field)
	{
		switch (Field)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return &Cache.Location;
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return &Cache.Rotation;
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return &Cache.Scale;
		default:
			return nullptr;
		}
	}

	const FComponentTransformDetailsBridge::FCachedTransformField* GetCachedField(
		const FComponentTransformDetailsBridge::FDetailsViewTransformCache& Cache,
		const FValueLadderPropertyTarget::ETransformField Field)
	{
		switch (Field)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return &Cache.Location;
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return &Cache.Rotation;
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return &Cache.Scale;
		default:
			return nullptr;
		}
	}

}

FComponentTransformDetailsBridge& FComponentTransformDetailsBridge::Get()
{
	static FComponentTransformDetailsBridge Instance;
	return Instance;
}

void FComponentTransformDetailsBridge::Register(FPropertyEditorModule& PropertyModule)
{
	if (RowExtensionDelegateHandle.IsValid())
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Row extension delegate already registered."));
		return;
	}

	RowExtensionDelegateHandle = PropertyModule.GetGlobalRowExtensionDelegate().AddRaw(this, &FComponentTransformDetailsBridge::HandleGenerateGlobalRowExtension);
	UE_LOG(LogValueLadder, Display, TEXT("[TransformBridge] Registered PropertyEditor global row extension delegate."));
}

void FComponentTransformDetailsBridge::Unregister(FPropertyEditorModule& PropertyModule)
{
	if (!RowExtensionDelegateHandle.IsValid())
	{
		return;
	}

	PropertyModule.GetGlobalRowExtensionDelegate().Remove(RowExtensionDelegateHandle);
	RowExtensionDelegateHandle.Reset();
	CachedFieldsByDetailsView.Reset();
	UE_LOG(LogValueLadder, Display, TEXT("[TransformBridge] Unregistered PropertyEditor global row extension delegate."));
}

bool FComponentTransformDetailsBridge::ResolveTargetFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget)
{
	CompactStaleEntries();

	FValueLadderPropertyTarget::ETransformField Field = FValueLadderPropertyTarget::ETransformField::Location;
	if (!TryResolveFieldFromWidgetPath(WidgetPath, Field))
	{
		return false;
	}

	FName ComponentName;
	FString ContainerType;
	int32 DisplayIndex = INDEX_NONE;
	int32 ComponentIndex = INDEX_NONE;
	if (!TryResolveComponentFromWidgetPath(WidgetPath, Field, ComponentName, ContainerType, DisplayIndex, ComponentIndex))
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Widget path matched %s row but did not resolve a component entry."), ToTransformFieldString(Field));
		return false;
	}

	const SWidget* const DetailsViewWidgetKey = FindDetailsViewWidgetKey(WidgetPath);
	if (DetailsViewWidgetKey == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] Failed to resolve owning details view from widget path for %s.%s"), ToTransformFieldString(Field), *ComponentName.ToString());
		return false;
	}

	FDetailsViewTransformCache* const DetailsViewCache = CachedFieldsByDetailsView.Find(DetailsViewWidgetKey);
	if (DetailsViewCache == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] No cached details-view entry for %s.%s in detailsView=%p."), ToTransformFieldString(Field), *ComponentName.ToString(), DetailsViewWidgetKey);
		return false;
	}

	const TSharedPtr<SWidget> CachedDetailsViewWidget = DetailsViewCache->DetailsViewWidget.Pin();
	if (!CachedDetailsViewWidget.IsValid() || CachedDetailsViewWidget.Get() != DetailsViewWidgetKey)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] Evicted stale details-view cache entry for key=%p while resolving %s.%s."), DetailsViewWidgetKey, ToTransformFieldString(Field), *ComponentName.ToString());
		CachedFieldsByDetailsView.Remove(DetailsViewWidgetKey);
		return false;
	}

	FCachedTransformField* const CachedField = GetMutableCachedField(*DetailsViewCache, Field);
	if (CachedField == nullptr || !IsCachedFieldValid(*CachedField))
	{
		if (CachedField != nullptr)
		{
			*CachedField = FCachedTransformField();
		}

		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] No valid cached property handle for %s in detailsView=%p when resolving component %s."), ToTransformFieldString(Field), DetailsViewWidgetKey, *ComponentName.ToString());
		return false;
	}

	const TSharedPtr<IPropertyHandle> LeafHandle = FindChildHandleByPropertyName(CachedField->PropertyHandle.ToSharedRef(), ComponentName);
	if (!LeafHandle.IsValid() || !LeafHandle->IsValidHandle())
	{
		*CachedField = FCachedTransformField();
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] Cached %s handle is missing child component %s."), ToTransformFieldString(Field), *ComponentName.ToString());
		return false;
	}

	EValueLadderNumericType NumericType = CachedField->NumericType;
	if (!ResolveNumericType(LeafHandle.ToSharedRef(), NumericType))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] Failed to resolve numeric type for %s.%s"), ToTransformFieldString(Field), *ComponentName.ToString());
		return false;
	}

	OutTarget = FValueLadderPropertyTarget();
	OutTarget.PropertyHandle = LeafHandle;
	OutTarget.NumericType = NumericType;
	OutTarget.bIsVectorComponent = true;
	OutTarget.TransformField = Field;
	OutTarget.ComponentName = ComponentName;

	UE_LOG(
		LogValueLadder,
		Display,
		TEXT("[TransformBridge] Resolved live widget path to %s.%s via detailsView=%p container=%s displayIndex=%d componentIndex=%d type=%s"),
		ToTransformFieldString(Field),
		*ComponentName.ToString(),
		DetailsViewWidgetKey,
		*ContainerType,
		DisplayIndex,
		ComponentIndex,
		ToNumericTypeString(NumericType));

	return true;
}

void FComponentTransformDetailsBridge::HandleGenerateGlobalRowExtension(
	const FOnGenerateGlobalRowExtensionArgs& InArgs,
	TArray<FPropertyRowExtensionButton>& OutExtensions)
{
	if (!InArgs.PropertyHandle.IsValid() || !InArgs.PropertyHandle->IsValidHandle())
	{
		return;
	}

	const TSharedPtr<IDetailTreeNode> OwnerTreeNode = InArgs.OwnerTreeNode.Pin();
	if (!OwnerTreeNode.IsValid())
	{
		return;
	}

	const TSharedPtr<IDetailsView> DetailsView = OwnerTreeNode->GetNodeDetailsViewSharedPtr();
	if (!DetailsView.IsValid())
	{
		return;
	}

	const FProperty* Property = InArgs.PropertyHandle->GetProperty();
	if (Property == nullptr)
	{
		return;
	}

	const FName PropertyName = Property->GetFName();
	if (PropertyName == USceneComponent::GetRelativeLocationPropertyName())
	{
		CacheTransformField(DetailsView.ToSharedRef(), FValueLadderPropertyTarget::ETransformField::Location, InArgs.PropertyHandle.ToSharedRef());
	}
	else if (PropertyName == USceneComponent::GetRelativeRotationPropertyName())
	{
		CacheTransformField(DetailsView.ToSharedRef(), FValueLadderPropertyTarget::ETransformField::Rotation, InArgs.PropertyHandle.ToSharedRef());
	}
	else if (PropertyName == USceneComponent::GetRelativeScale3DPropertyName())
	{
		CacheTransformField(DetailsView.ToSharedRef(), FValueLadderPropertyTarget::ETransformField::Scale, InArgs.PropertyHandle.ToSharedRef());
	}
}

void FComponentTransformDetailsBridge::CacheTransformField(
	const TSharedRef<IDetailsView>& DetailsView,
	const FValueLadderPropertyTarget::ETransformField Field,
	const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	CompactStaleEntries();

	const FName ComponentPropertyName = Field == FValueLadderPropertyTarget::ETransformField::Rotation ? VLT_NAME_Roll : VLT_NAME_X;
	const TSharedPtr<IPropertyHandle> ComponentHandle = FindChildHandleByPropertyName(PropertyHandle, ComponentPropertyName);
	if (!ComponentHandle.IsValid() || !ComponentHandle->IsValidHandle())
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Skipped caching %s because component %s was unavailable."), ToTransformFieldString(Field), *ComponentPropertyName.ToString());
		return;
	}

	EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
	if (!ResolveNumericType(ComponentHandle.ToSharedRef(), NumericType))
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Skipped caching %s because child numeric type was unsupported."), ToTransformFieldString(Field));
		return;
	}

	const SWidget* const DetailsViewWidgetKey = GetDetailsViewWidgetKey(DetailsView);
	if (DetailsViewWidgetKey == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[TransformBridge] Skipped caching %s because owning details view widget key was unavailable."), ToTransformFieldString(Field));
		return;
	}

	FDetailsViewTransformCache& DetailsViewCache = CachedFieldsByDetailsView.FindOrAdd(DetailsViewWidgetKey);
	DetailsViewCache.DetailsViewWidget = StaticCastSharedRef<SWidget>(DetailsView);
	FCachedTransformField* const CachedField = GetMutableCachedField(DetailsViewCache, Field);
	if (CachedField == nullptr)
	{
		return;
	}

	CachedField->PropertyHandle = PropertyHandle;
	CachedField->NumericType = NumericType;
	UE_LOG(LogValueLadder, Display, TEXT("[TransformBridge] Cached %s property handle detailsView=%p identifier=%s displayName='%s' type=%s"), ToTransformFieldString(Field), DetailsViewWidgetKey, *DetailsView->GetIdentifier().ToString(), *PropertyHandle->GetPropertyDisplayName().ToString(), ToNumericTypeString(NumericType));
}

void FComponentTransformDetailsBridge::CompactStaleEntries()
{
	int32 RemovedCount = 0;
	for (auto It = CachedFieldsByDetailsView.CreateIterator(); It; ++It)
	{
		const TSharedPtr<SWidget> DetailsViewWidget = It.Value().DetailsViewWidget.Pin();
		if (!DetailsViewWidget.IsValid() || DetailsViewWidget.Get() != It.Key())
		{
			UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Compact removed stale details-view cache entry key=%p weakValid=%s pointerMatch=%s"), It.Key(), DetailsViewWidget.IsValid() ? TEXT("true") : TEXT("false"), (DetailsViewWidget.IsValid() && DetailsViewWidget.Get() == It.Key()) ? TEXT("true") : TEXT("false"));
			It.RemoveCurrent();
			++RemovedCount;
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[TransformBridge] Compact removed %d stale details-view cache entries."), RemovedCount);
	}
}

bool FComponentTransformDetailsBridge::ResolveNumericType(
	const TSharedRef<IPropertyHandle>& PropertyHandle,
	EValueLadderNumericType& OutType)
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

	if (Property->IsA<FIntProperty>())
	{
		OutType = EValueLadderNumericType::Int32;
		return true;
	}

	return false;
}

TSharedPtr<IPropertyHandle> FComponentTransformDetailsBridge::FindChildHandleByPropertyName(
	const TSharedRef<IPropertyHandle>& PropertyHandle,
	const FName& PropertyName)
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

void FComponentTransformDetailsBridge::CollectWidgetsByTypePrefix(
	const TSharedRef<SWidget>& RootWidget,
	const FString& TypePrefix,
	TArray<TSharedRef<SWidget>>& OutWidgets)
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

const SWidget* FComponentTransformDetailsBridge::GetDetailsViewWidgetKey(const TSharedRef<IDetailsView>& DetailsView)
{
	return &StaticCastSharedRef<SWidget>(DetailsView).Get();
}

const SWidget* FComponentTransformDetailsBridge::FindDetailsViewWidgetKey(const FWidgetPath& WidgetPath)
{
	for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
		const FString WidgetType = Widget->GetTypeAsString();
		if (WidgetType.StartsWith(TEXT("SDetailsView")))
		{
			return &Widget.Get();
		}
	}

	return nullptr;
}

bool FComponentTransformDetailsBridge::TryResolveFieldFromWidgetPath(
	const FWidgetPath& WidgetPath,
	FValueLadderPropertyTarget::ETransformField& OutField)
{
	for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
		const TArray<TSharedRef<FTagMetaData>> AllTags = Widget->GetAllMetaData<FTagMetaData>();
		for (const TSharedRef<FTagMetaData>& TagMetaData : AllTags)
		{
			if (TryMapRowTagToField(TagMetaData->Tag, OutField))
			{
				return true;
			}
		}
	}

	return false;
}

bool FComponentTransformDetailsBridge::TryResolveComponentFromWidgetPath(
	const FWidgetPath& WidgetPath,
	const FValueLadderPropertyTarget::ETransformField Field,
	FName& OutComponentName,
	FString& OutContainerType,
	int32& OutDisplayIndex,
	int32& OutComponentIndex)
{
	TSharedPtr<SWidget> ActiveNumericEntry;
	TSharedPtr<SWidget> ContainerWidget;
	const FString ExpectedContainerPrefix = Field == FValueLadderPropertyTarget::ETransformField::Rotation
		? TEXT("SNumericRotatorInputBox<")
		: TEXT("SNumericVectorInputBox<");

	for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
		const FString WidgetType = Widget->GetTypeAsString();
		if (!ActiveNumericEntry.IsValid() && WidgetType.StartsWith(TEXT("SNumericEntryBox<")))
		{
			ActiveNumericEntry = Widget;
		}

		if (!ContainerWidget.IsValid() && WidgetType.StartsWith(ExpectedContainerPrefix))
		{
			ContainerWidget = Widget;
		}
	}

	if (!ActiveNumericEntry.IsValid() || !ContainerWidget.IsValid())
	{
		return false;
	}

	TArray<TSharedRef<SWidget>> NumericEntryWidgets;
	CollectWidgetsByTypePrefix(ContainerWidget.ToSharedRef(), TEXT("SNumericEntryBox<"), NumericEntryWidgets);
	OutDisplayIndex = NumericEntryWidgets.IndexOfByPredicate(
		[&ActiveNumericEntry](const TSharedRef<SWidget>& Widget)
		{
			return &Widget.Get() == ActiveNumericEntry.Get();
		});

	if (!NumericEntryWidgets.IsValidIndex(OutDisplayIndex))
	{
		return false;
	}

	const FIntVector4 Swizzle = AxisDisplayInfo::GetTransformAxisSwizzle();
	if (OutDisplayIndex < 0 || OutDisplayIndex > 2)
	{
		return false;
	}

	OutComponentIndex = Swizzle[OutDisplayIndex];
	const FName VectorComponentNames[3] = {VLT_NAME_X, VLT_NAME_Y, VLT_NAME_Z};
	const FName RotationComponentNames[3] = {VLT_NAME_Roll, VLT_NAME_Pitch, VLT_NAME_Yaw};
	const FName* ComponentNames = Field == FValueLadderPropertyTarget::ETransformField::Rotation
		? RotationComponentNames
		: VectorComponentNames;

	if (OutComponentIndex < 0 || OutComponentIndex > 2)
	{
		return false;
	}

	OutComponentName = ComponentNames[OutComponentIndex];
	OutContainerType = ContainerWidget->GetTypeAsString();
	return true;
}
