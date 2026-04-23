#include "Input/ValueLadderTargetRegistry.h"

#include "ValueLadderLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/SWidget.h"

namespace
{
	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		return ValueLadder::ToNumericTypeString(NumericType);
	}

	const TCHAR* ToTargetKindString(const FValueLadderPropertyTarget::ETargetKind Kind)
	{
		switch (Kind)
		{
		case FValueLadderPropertyTarget::ETargetKind::PropertyHandleScalar:
			return TEXT("PropertyHandleScalar");
		case FValueLadderPropertyTarget::ETargetKind::TransformProxy:
			return TEXT("TransformProxy");
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

	constexpr const TCHAR* ValueLadderHandleTagPrefix = TEXT("ValueLadder.Handle.");
	constexpr const TCHAR* DetailRowTagPrefix = TEXT("DetailRowItem.");

	bool TryParseHandleTag(const FName Tag, FValueLadderTargetHandle& OutHandle)
	{
		const FString TagString = Tag.ToString();
		if (!TagString.StartsWith(ValueLadderHandleTagPrefix))
		{
			return false;
		}

		const TCHAR* const NumberStart = *TagString + FCString::Strlen(ValueLadderHandleTagPrefix);
		TCHAR* EndPtr = nullptr;
		const uint64 ParsedHandle = FCString::Strtoui64(NumberStart, &EndPtr, 10);
		if (EndPtr == nullptr || *EndPtr != TEXT('\0'))
		{
			return false;
		}

		OutHandle = ParsedHandle;
		return true;
	}

	bool TryExtractDetailRowToken(const FWidgetPath& WidgetPath, FString& OutToken)
	{
		for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
			const auto TryConsumeTag = [&OutToken](const FName& Tag)
			{
				const FString TagString = Tag.ToString();
				if (!TagString.StartsWith(DetailRowTagPrefix))
				{
					return false;
				}

				OutToken = TagString.Mid(FCString::Strlen(DetailRowTagPrefix));
				return !OutToken.IsEmpty();
			};

			if (!Widget->GetTag().IsNone() && TryConsumeTag(Widget->GetTag()))
			{
				return true;
			}

			for (const TSharedRef<FTagMetaData>& TagMetaData : Widget->GetAllMetaData<FTagMetaData>())
			{
				if (TryConsumeTag(TagMetaData->Tag))
				{
					return true;
				}
			}
		}

		return false;
	}

	FString NormalizeRowToken(const FString& InToken)
	{
		FString Normalized;
		Normalized.Reserve(InToken.Len());
		for (const TCHAR Character : InToken)
		{
			if (FChar::IsAlnum(Character))
			{
				Normalized.AppendChar(FChar::ToLower(Character));
			}
		}

		return Normalized;
	}

	bool AreEquivalentPropertyHandles(const TSharedPtr<IPropertyHandle>& LeftHandle, const TSharedPtr<IPropertyHandle>& RightHandle)
	{
		if (!LeftHandle.IsValid() || !RightHandle.IsValid() || !LeftHandle->IsValidHandle() || !RightHandle->IsValidHandle())
		{
			return false;
		}

		if (LeftHandle == RightHandle)
		{
			return true;
		}

		const FProperty* const LeftProperty = LeftHandle->GetProperty();
		const FProperty* const RightProperty = RightHandle->GetProperty();
		if (LeftProperty == nullptr || RightProperty == nullptr || LeftProperty != RightProperty)
		{
			return false;
		}

		return LeftHandle->GeneratePathToProperty() == RightHandle->GeneratePathToProperty();
	}

	FString DescribeWidget(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return TEXT("widget=<invalid>");
		}

		return FString::Printf(
			TEXT("widget=%p type=%s tag=%s"),
			Widget.Get(),
			*Widget->GetTypeAsString(),
			*Widget->GetTag().ToString());
	}

	FString DescribeTarget(const FValueLadderPropertyTarget& Target)
	{
		const bool bHandleValid = Target.PropertyHandle.IsValid() && Target.PropertyHandle->IsValidHandle();
		const FString PropertyDisplayName = bHandleValid ? Target.PropertyHandle->GetPropertyDisplayName().ToString() : TEXT("<invalid>");

		return FString::Printf(
			TEXT("targetValid=%s kind=%s property=%s type=%s vector=%s field=%s component=%s"),
			bHandleValid ? TEXT("true") : TEXT("false"),
			ToTargetKindString(Target.Kind),
			*PropertyDisplayName,
			ToNumericTypeString(Target.NumericType),
			Target.bIsVectorComponent ? TEXT("true") : TEXT("false"),
			ToTransformFieldString(Target.TransformField),
			*Target.ComponentName.ToString());
	}
}

FValueLadderTargetRegistry& FValueLadderTargetRegistry::Get()
{
	static FValueLadderTargetRegistry Instance;
	return Instance;
}

FValueLadderTargetHandle FValueLadderTargetRegistry::RegisterTarget(
	const TSharedRef<SWidget>& Widget,
	const FValueLadderPropertyTarget& Target)
{
	FScopeLock Lock(&RegistryMutex);
	Compact_NoLock();

	const FValueLadderTargetHandle NewHandle = NextHandle++;
	FRegisteredTarget& RegisteredTarget = RegisteredTargets.Add(NewHandle);
	RegisteredTarget.Widget = Widget;
	RegisteredTarget.Target = Target;
	UE_LOG(
		LogValueLadder,
		Display,
		TEXT("[Registry] Registered handle=%llu widget=%p type=%s vector=%s total=%d"),
		static_cast<uint64>(NewHandle),
		static_cast<const void*>(&Widget.Get()),
		ToNumericTypeString(Target.NumericType),
		Target.bIsVectorComponent ? TEXT("true") : TEXT("false"),
		RegisteredTargets.Num());

	return NewHandle;
}

void FValueLadderTargetRegistry::UnregisterTarget(const FValueLadderTargetHandle Handle)
{
	FScopeLock Lock(&RegistryMutex);
	UE_LOG(LogValueLadder, Display, TEXT("[Registry] Unregister handle=%llu existed=%s beforeTotal=%d"), static_cast<uint64>(Handle), RegisteredTargets.Contains(Handle) ? TEXT("true") : TEXT("false"), RegisteredTargets.Num());
	RegisteredTargets.Remove(Handle);
}

bool FValueLadderTargetRegistry::ResolveTargetFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget)
{
	FScopeLock Lock(&RegistryMutex);
	Compact_NoLock();
	OutTarget = FValueLadderPropertyTarget();

	int32 WidgetMatchCount = 0;
	int32 InvalidHandleMatchCount = 0;

	for (int32 WidgetPathIndex = WidgetPath.Widgets.Num() - 1; WidgetPathIndex >= 0; --WidgetPathIndex)
	{
		const TSharedRef<SWidget>& CandidateWidget = WidgetPath.Widgets[WidgetPathIndex].Widget;

		for (const TPair<FValueLadderTargetHandle, FRegisteredTarget>& Entry : RegisteredTargets)
		{
			const TSharedPtr<SWidget> RegisteredWidget = Entry.Value.Widget.Pin();
			if (!RegisteredWidget.IsValid())
			{
				continue;
			}

			if (RegisteredWidget.Get() == &CandidateWidget.Get())
			{
				++WidgetMatchCount;
				OutTarget = Entry.Value.Target;
				const bool bPropertyHandleValid = OutTarget.PropertyHandle.IsValid();
				const bool bIsValidHandle = bPropertyHandleValid && OutTarget.PropertyHandle->IsValidHandle();

				if (!bIsValidHandle)
				{
					++InvalidHandleMatchCount;
					UE_LOG(
						LogValueLadder,
						Verbose,
						TEXT("[Registry] Resolve matched widget with invalid property handle. handle=%llu pathIndex=%d %s %s"),
						static_cast<uint64>(Entry.Key),
						WidgetPathIndex,
						*DescribeWidget(RegisteredWidget),
						*DescribeTarget(OutTarget));
					continue;
				}

				return true;
			}
		}
	}

	for (int32 WidgetPathIndex = WidgetPath.Widgets.Num() - 1; WidgetPathIndex >= 0; --WidgetPathIndex)
	{
		const TSharedRef<SWidget>& CandidateWidget = WidgetPath.Widgets[WidgetPathIndex].Widget;
		for (const TSharedRef<FTagMetaData>& TagMetaData : CandidateWidget->GetAllMetaData<FTagMetaData>())
		{
			FValueLadderTargetHandle TaggedHandle = 0;
			if (!TryParseHandleTag(TagMetaData->Tag, TaggedHandle))
			{
				continue;
			}

			const FRegisteredTarget* TaggedTarget = RegisteredTargets.Find(TaggedHandle);
			if (TaggedTarget == nullptr)
			{
				continue;
			}

			OutTarget = TaggedTarget->Target;
			const bool bPropertyHandleValid = OutTarget.PropertyHandle.IsValid();
			const bool bIsValidHandle = bPropertyHandleValid && OutTarget.PropertyHandle->IsValidHandle();
			if (!bIsValidHandle)
			{
				++InvalidHandleMatchCount;
				UE_LOG(
					LogValueLadder,
					Verbose,
					TEXT("[Registry] Resolve matched tagged widget with invalid property handle. handle=%llu pathIndex=%d tag=%s %s"),
					static_cast<uint64>(TaggedHandle),
					WidgetPathIndex,
					*TagMetaData->Tag.ToString(),
					*DescribeTarget(OutTarget));
				continue;
			}

			UE_LOG(
				LogValueLadder,
				Verbose,
				TEXT("[Registry] Resolve fell back to tagged widget handle=%llu pathIndex=%d tag=%s"),
				static_cast<uint64>(TaggedHandle),
				WidgetPathIndex,
				*TagMetaData->Tag.ToString());
			return true;
		}
	}

	FString DetailRowToken;
	if (TryExtractDetailRowToken(WidgetPath, DetailRowToken))
	{
		const FString NormalizedDetailRowToken = NormalizeRowToken(DetailRowToken);
		const FRegisteredTarget* PropertyNameMatch = nullptr;
		const FRegisteredTarget* DisplayNameMatch = nullptr;
		bool bPropertyNameAmbiguous = false;
		bool bDisplayNameAmbiguous = false;
		for (const TPair<FValueLadderTargetHandle, FRegisteredTarget>& Entry : RegisteredTargets)
		{
			const FRegisteredTarget& RegisteredTarget = Entry.Value;
			if (!RegisteredTarget.Target.PropertyHandle.IsValid() || !RegisteredTarget.Target.PropertyHandle->IsValidHandle())
			{
				continue;
			}

			const FProperty* Property = RegisteredTarget.Target.PropertyHandle->GetProperty();
			if (Property == nullptr)
			{
				continue;
			}

			if (NormalizeRowToken(Property->GetFName().ToString()) == NormalizedDetailRowToken)
			{
				if (PropertyNameMatch != nullptr
					&& !AreEquivalentPropertyHandles(PropertyNameMatch->Target.PropertyHandle, RegisteredTarget.Target.PropertyHandle))
				{
					bPropertyNameAmbiguous = true;
				}
				PropertyNameMatch = &RegisteredTarget;
			}

			if (NormalizeRowToken(RegisteredTarget.Target.PropertyHandle->GetPropertyDisplayName().ToString()) == NormalizedDetailRowToken)
			{
				if (DisplayNameMatch != nullptr
					&& !AreEquivalentPropertyHandles(DisplayNameMatch->Target.PropertyHandle, RegisteredTarget.Target.PropertyHandle))
				{
					bDisplayNameAmbiguous = true;
				}
				DisplayNameMatch = &RegisteredTarget;
			}
		}

		if (PropertyNameMatch != nullptr && !bPropertyNameAmbiguous)
		{
			OutTarget = PropertyNameMatch->Target;
			return true;
		}

		if (DisplayNameMatch != nullptr && !bDisplayNameAmbiguous)
		{
			OutTarget = DisplayNameMatch->Target;
			return true;
		}
	}

	if (InvalidHandleMatchCount > 0)
	{
		UE_LOG(
			LogValueLadder,
			Verbose,
			TEXT("[Registry] Resolve failed after stale widget matches. pathDepth=%d widgetMatches=%d invalidHandleMatches=%d registeredTargets=%d"),
			WidgetPath.Widgets.Num(),
			WidgetMatchCount,
			InvalidHandleMatchCount,
			RegisteredTargets.Num());
	}

	OutTarget = FValueLadderPropertyTarget();

	return false;
}

bool FValueLadderTargetRegistry::FindRegisteredWidgetForPropertyName(
	const FName PropertyName,
	TSharedPtr<SWidget>& OutWidget,
	FValueLadderPropertyTarget& OutTarget)
{
	FScopeLock Lock(&RegistryMutex);
	Compact_NoLock();

	OutWidget.Reset();
	OutTarget = FValueLadderPropertyTarget();

	for (const TPair<FValueLadderTargetHandle, FRegisteredTarget>& Entry : RegisteredTargets)
	{
		const TSharedPtr<SWidget> RegisteredWidget = Entry.Value.Widget.Pin();
		if (!RegisteredWidget.IsValid())
		{
			continue;
		}

		const TSharedPtr<IPropertyHandle> PropertyHandle = Entry.Value.Target.PropertyHandle;
		if (!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
		{
			continue;
		}

		const FProperty* Property = PropertyHandle->GetProperty();
		if (Property == nullptr || Property->GetFName() != PropertyName)
		{
			continue;
		}

		OutWidget = RegisteredWidget;
		OutTarget = Entry.Value.Target;
		return true;
	}

	return false;
}

void FValueLadderTargetRegistry::Compact_NoLock()
{
	const int32 BeforeCount = RegisteredTargets.Num();
	int32 RemovedCount = 0;
	int32 InvalidWidgetCount = 0;
	int32 InvalidTargetCount = 0;
	int32 InvalidBothCount = 0;

	for (auto It = RegisteredTargets.CreateIterator(); It; ++It)
	{
		const TSharedPtr<SWidget> RegisteredWidget = It.Value().Widget.Pin();
		const bool bWidgetValid = RegisteredWidget.IsValid();

		const bool bTargetValid = It.Value().Target.IsValid();
		if (!bWidgetValid || !bTargetValid)
		{
			const TCHAR* Reason = TEXT("unknown");
			if (!bWidgetValid && !bTargetValid)
			{
				Reason = TEXT("widget+target-invalid");
				++InvalidBothCount;
			}
			else if (!bWidgetValid)
			{
				Reason = TEXT("widget-invalid");
				++InvalidWidgetCount;
			}
			else
			{
				Reason = TEXT("target-invalid");
				++InvalidTargetCount;
			}

			UE_LOG(
				LogValueLadder,
				Verbose,
				TEXT("[Registry] Compact removed stale handle=%llu reason=%s widgetValid=%s targetValid=%s"),
				static_cast<uint64>(It.Key()),
				Reason,
				bWidgetValid ? TEXT("true") : TEXT("false"),
				bTargetValid ? TEXT("true") : TEXT("false"));
			++RemovedCount;
			It.RemoveCurrent();
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(
			LogValueLadder,
			Verbose,
			TEXT("[Registry] Compact summary: before=%d removed=%d after=%d invalidWidget=%d invalidTarget=%d invalidBoth=%d"),
			BeforeCount,
			RemovedCount,
			RegisteredTargets.Num(),
			InvalidWidgetCount,
			InvalidTargetCount,
			InvalidBothCount);
	}
}
