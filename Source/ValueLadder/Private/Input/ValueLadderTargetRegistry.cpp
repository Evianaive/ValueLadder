#include "Input/ValueLadderTargetRegistry.h"

#include "HAL/PlatformTime.h"
#include "ValueLadderLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/SWidget.h"

namespace
{
	constexpr double RegistryPerfLogThresholdUs = 250.0;
	constexpr int32 RegistryCompactMutationThreshold = 512;
	constexpr int32 RegistryCompactStaleHintThreshold = 32;

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

	void AddHandleToIndex(TMap<const SWidget*, TArray<FValueLadderTargetHandle>>& Index, const SWidget* const Key, const FValueLadderTargetHandle Handle)
	{
		if (Key == nullptr)
		{
			return;
		}

		Index.FindOrAdd(Key).AddUnique(Handle);
	}

	void AddHandleToIndex(TMap<FString, TArray<FValueLadderTargetHandle>>& Index, const FString& Key, const FValueLadderTargetHandle Handle)
	{
		if (Key.IsEmpty())
		{
			return;
		}

		Index.FindOrAdd(Key).AddUnique(Handle);
	}

	void RemoveHandleFromIndex(TMap<const SWidget*, TArray<FValueLadderTargetHandle>>& Index, const SWidget* const Key, const FValueLadderTargetHandle Handle)
	{
		if (Key == nullptr)
		{
			return;
		}

		TArray<FValueLadderTargetHandle>* const Handles = Index.Find(Key);
		if (Handles == nullptr)
		{
			return;
		}

		Handles->RemoveSingleSwap(Handle);
		if (Handles->Num() == 0)
		{
			Index.Remove(Key);
		}
	}

	void RemoveHandleFromIndex(TMap<FString, TArray<FValueLadderTargetHandle>>& Index, const FString& Key, const FValueLadderTargetHandle Handle)
	{
		if (Key.IsEmpty())
		{
			return;
		}

		TArray<FValueLadderTargetHandle>* const Handles = Index.Find(Key);
		if (Handles == nullptr)
		{
			return;
		}

		Handles->RemoveSingleSwap(Handle);
		if (Handles->Num() == 0)
		{
			Index.Remove(Key);
		}
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
	const double StartTimeSeconds = FPlatformTime::Seconds();
	FScopeLock Lock(&RegistryMutex);

	const FValueLadderTargetHandle NewHandle = NextHandle++;
	FRegisteredTarget& RegisteredTarget = RegisteredTargets.Add(NewHandle);
	RegisteredTarget.WidgetKey = &Widget.Get();
	RegisteredTarget.Widget = Widget;
	RegisteredTarget.Target = Target;
	if (Target.PropertyHandle.IsValid() && Target.PropertyHandle->IsValidHandle())
	{
		if (const FProperty* const Property = Target.PropertyHandle->GetProperty())
		{
			RegisteredTarget.NormalizedPropertyToken = NormalizeRowToken(Property->GetFName().ToString());
		}

		RegisteredTarget.NormalizedDisplayToken = NormalizeRowToken(Target.PropertyHandle->GetPropertyDisplayName().ToString());
	}

	AddHandleToIndex(WidgetIndex, RegisteredTarget.WidgetKey, NewHandle);
	AddHandleToIndex(PropertyTokenIndex, RegisteredTarget.NormalizedPropertyToken, NewHandle);
	AddHandleToIndex(DisplayTokenIndex, RegisteredTarget.NormalizedDisplayToken, NewHandle);
	++MutationsSinceCompact;
	UE_LOG(
		LogValueLadder,
		Display,
		TEXT("[Registry] Registered handle=%llu widget=%p type=%s vector=%s total=%d"),
			static_cast<uint64>(NewHandle),
			static_cast<const void*>(&Widget.Get()),
			ToNumericTypeString(Target.NumericType),
			Target.bIsVectorComponent ? TEXT("true") : TEXT("false"),
			RegisteredTargets.Num());

	const double DurationUs = (FPlatformTime::Seconds() - StartTimeSeconds) * 1000000.0;
	if (DurationUs >= RegistryPerfLogThresholdUs)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Perf][Registry] RegisterTarget duration=%.1fus compact=%.1fus total=%d"), DurationUs, 0.0, RegisteredTargets.Num());
	}
	else
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Perf][Registry] RegisterTarget duration=%.1fus compact=%.1fus total=%d"), DurationUs, 0.0, RegisteredTargets.Num());
	}

	return NewHandle;
}

void FValueLadderTargetRegistry::UnregisterTarget(const FValueLadderTargetHandle Handle)
{
	FScopeLock Lock(&RegistryMutex);
	UE_LOG(LogValueLadder, Display, TEXT("[Registry] Unregister handle=%llu existed=%s beforeTotal=%d"), static_cast<uint64>(Handle), RegisteredTargets.Contains(Handle) ? TEXT("true") : TEXT("false"), RegisteredTargets.Num());
	if (const FRegisteredTarget* const RegisteredTarget = RegisteredTargets.Find(Handle))
	{
		RemoveHandleFromIndex(WidgetIndex, RegisteredTarget->WidgetKey, Handle);
		RemoveHandleFromIndex(PropertyTokenIndex, RegisteredTarget->NormalizedPropertyToken, Handle);
		RemoveHandleFromIndex(DisplayTokenIndex, RegisteredTarget->NormalizedDisplayToken, Handle);
		RegisteredTargets.Remove(Handle);
		++MutationsSinceCompact;
	}
}

bool FValueLadderTargetRegistry::ResolveTargetFromWidgetPath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget)
{
	const double StartTimeSeconds = FPlatformTime::Seconds();
	FScopeLock Lock(&RegistryMutex);
	const double CompactStartTimeSeconds = FPlatformTime::Seconds();
	MaybeCompact_NoLock();
	const double CompactDurationUs = (FPlatformTime::Seconds() - CompactStartTimeSeconds) * 1000000.0;
	OutTarget = FValueLadderPropertyTarget();

	int32 WidgetMatchCount = 0;
	int32 InvalidHandleMatchCount = 0;
	const auto MarkStale = [&]()
	{
		bCompactRequested = true;
		++StaleHintCount;
	};
	const auto LogResolvePerf = [&](const TCHAR* Result)
	{
		const double DurationUs = (FPlatformTime::Seconds() - StartTimeSeconds) * 1000000.0;
		if (DurationUs >= RegistryPerfLogThresholdUs)
		{
			UE_LOG(LogValueLadder, Display, TEXT("[Perf][Registry] ResolveTargetFromWidgetPath result=%s duration=%.1fus compact=%.1fus pathDepth=%d registered=%d widgetMatches=%d invalidMatches=%d"), Result, DurationUs, CompactDurationUs, WidgetPath.Widgets.Num(), RegisteredTargets.Num(), WidgetMatchCount, InvalidHandleMatchCount);
		}
		else
		{
			UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Perf][Registry] ResolveTargetFromWidgetPath result=%s duration=%.1fus compact=%.1fus pathDepth=%d registered=%d widgetMatches=%d invalidMatches=%d"), Result, DurationUs, CompactDurationUs, WidgetPath.Widgets.Num(), RegisteredTargets.Num(), WidgetMatchCount, InvalidHandleMatchCount);
		}
	};

	for (int32 WidgetPathIndex = WidgetPath.Widgets.Num() - 1; WidgetPathIndex >= 0; --WidgetPathIndex)
	{
		const TSharedRef<SWidget>& CandidateWidget = WidgetPath.Widgets[WidgetPathIndex].Widget;
		const TArray<FValueLadderTargetHandle>* const IndexedHandles = WidgetIndex.Find(&CandidateWidget.Get());
		if (IndexedHandles == nullptr)
		{
			continue;
		}

		for (int32 HandleIndex = IndexedHandles->Num() - 1; HandleIndex >= 0; --HandleIndex)
		{
			const FValueLadderTargetHandle IndexedHandle = (*IndexedHandles)[HandleIndex];
			const FRegisteredTarget* const RegisteredTarget = RegisteredTargets.Find(IndexedHandle);
			if (RegisteredTarget == nullptr)
			{
				++InvalidHandleMatchCount;
				MarkStale();
				continue;
			}

			const TSharedPtr<SWidget> RegisteredWidget = RegisteredTarget->Widget.Pin();
			if (!RegisteredWidget.IsValid())
			{
				++InvalidHandleMatchCount;
				MarkStale();
				continue;
			}

			++WidgetMatchCount;
			OutTarget = RegisteredTarget->Target;
			const bool bPropertyHandleValid = OutTarget.PropertyHandle.IsValid();
			const bool bIsValidHandle = bPropertyHandleValid && OutTarget.PropertyHandle->IsValidHandle();

			if (!bIsValidHandle)
			{
				++InvalidHandleMatchCount;
				MarkStale();
				UE_LOG(
					LogValueLadder,
					Verbose,
					TEXT("[Registry] Resolve matched widget with invalid property handle. handle=%llu pathIndex=%d %s %s"),
					static_cast<uint64>(IndexedHandle),
					WidgetPathIndex,
					*DescribeWidget(RegisteredWidget),
					*DescribeTarget(OutTarget));
				continue;
			}

			LogResolvePerf(TEXT("direct-widget"));
			return true;
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

			const FRegisteredTarget* const TaggedTarget = RegisteredTargets.Find(TaggedHandle);
			if (TaggedTarget == nullptr)
			{
				MarkStale();
				continue;
			}

			OutTarget = TaggedTarget->Target;
			const bool bPropertyHandleValid = OutTarget.PropertyHandle.IsValid();
			const bool bIsValidHandle = bPropertyHandleValid && OutTarget.PropertyHandle->IsValidHandle();
			if (!bIsValidHandle)
			{
				++InvalidHandleMatchCount;
				MarkStale();
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
			LogResolvePerf(TEXT("tagged-widget"));
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

		if (const TArray<FValueLadderTargetHandle>* const PropertyHandles = PropertyTokenIndex.Find(NormalizedDetailRowToken))
		{
			for (const FValueLadderTargetHandle IndexedHandle : *PropertyHandles)
			{
				const FRegisteredTarget* const RegisteredTarget = RegisteredTargets.Find(IndexedHandle);
				if (RegisteredTarget == nullptr || !RegisteredTarget->Target.PropertyHandle.IsValid() || !RegisteredTarget->Target.PropertyHandle->IsValidHandle())
				{
					MarkStale();
					continue;
				}

				if (PropertyNameMatch != nullptr
					&& !AreEquivalentPropertyHandles(PropertyNameMatch->Target.PropertyHandle, RegisteredTarget->Target.PropertyHandle))
				{
					bPropertyNameAmbiguous = true;
				}
				PropertyNameMatch = RegisteredTarget;
			}
		}

		if (const TArray<FValueLadderTargetHandle>* const DisplayHandles = DisplayTokenIndex.Find(NormalizedDetailRowToken))
		{
			for (const FValueLadderTargetHandle IndexedHandle : *DisplayHandles)
			{
				const FRegisteredTarget* const RegisteredTarget = RegisteredTargets.Find(IndexedHandle);
				if (RegisteredTarget == nullptr || !RegisteredTarget->Target.PropertyHandle.IsValid() || !RegisteredTarget->Target.PropertyHandle->IsValidHandle())
				{
					MarkStale();
					continue;
				}

				if (DisplayNameMatch != nullptr
					&& !AreEquivalentPropertyHandles(DisplayNameMatch->Target.PropertyHandle, RegisteredTarget->Target.PropertyHandle))
				{
					bDisplayNameAmbiguous = true;
				}
				DisplayNameMatch = RegisteredTarget;
			}
		}

		if (PropertyNameMatch != nullptr && !bPropertyNameAmbiguous)
		{
			OutTarget = PropertyNameMatch->Target;
			LogResolvePerf(TEXT("detail-row-property"));
			return true;
		}

		if (DisplayNameMatch != nullptr && !bDisplayNameAmbiguous)
		{
			OutTarget = DisplayNameMatch->Target;
			LogResolvePerf(TEXT("detail-row-display"));
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
	LogResolvePerf(TEXT("miss"));

	return false;
}

bool FValueLadderTargetRegistry::FindRegisteredWidgetForPropertyName(
	const FName PropertyName,
	TSharedPtr<SWidget>& OutWidget,
	FValueLadderPropertyTarget& OutTarget)
{
	FScopeLock Lock(&RegistryMutex);
	MaybeCompact_NoLock();

	OutWidget.Reset();
	OutTarget = FValueLadderPropertyTarget();
	const FString NormalizedPropertyToken = NormalizeRowToken(PropertyName.ToString());
	const TArray<FValueLadderTargetHandle>* const PropertyHandles = PropertyTokenIndex.Find(NormalizedPropertyToken);
	if (PropertyHandles == nullptr)
	{
		return false;
	}

	for (const FValueLadderTargetHandle IndexedHandle : *PropertyHandles)
	{
		const FRegisteredTarget* const RegisteredTarget = RegisteredTargets.Find(IndexedHandle);
		if (RegisteredTarget == nullptr)
		{
			bCompactRequested = true;
			++StaleHintCount;
			continue;
		}

		const TSharedPtr<SWidget> RegisteredWidget = RegisteredTarget->Widget.Pin();
		if (!RegisteredWidget.IsValid())
		{
			bCompactRequested = true;
			++StaleHintCount;
			continue;
		}

		const TSharedPtr<IPropertyHandle> PropertyHandle = RegisteredTarget->Target.PropertyHandle;
		if (!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
		{
			bCompactRequested = true;
			++StaleHintCount;
			continue;
		}

		const FProperty* Property = PropertyHandle->GetProperty();
		if (Property == nullptr || Property->GetFName() != PropertyName)
		{
			continue;
		}

		OutWidget = RegisteredWidget;
		OutTarget = RegisteredTarget->Target;
		return true;
	}

	return false;
}

void FValueLadderTargetRegistry::MaybeCompact_NoLock(const bool bForce)
{
	if (!bForce)
	{
		const bool bThresholdReached = MutationsSinceCompact >= RegistryCompactMutationThreshold || StaleHintCount >= RegistryCompactStaleHintThreshold;
		if (!bCompactRequested && !bThresholdReached)
		{
			return;
		}
	}

	Compact_NoLock();
}

void FValueLadderTargetRegistry::Compact_NoLock()
{
	const double StartTimeSeconds = FPlatformTime::Seconds();
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

	WidgetIndex.Reset();
	PropertyTokenIndex.Reset();
	DisplayTokenIndex.Reset();
	for (const TPair<FValueLadderTargetHandle, FRegisteredTarget>& Entry : RegisteredTargets)
	{
		AddHandleToIndex(WidgetIndex, Entry.Value.WidgetKey, Entry.Key);
		AddHandleToIndex(PropertyTokenIndex, Entry.Value.NormalizedPropertyToken, Entry.Key);
		AddHandleToIndex(DisplayTokenIndex, Entry.Value.NormalizedDisplayToken, Entry.Key);
	}

	MutationsSinceCompact = 0;
	StaleHintCount = 0;
	bCompactRequested = false;

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

	const double DurationUs = (FPlatformTime::Seconds() - StartTimeSeconds) * 1000000.0;
	if (RemovedCount > 0 || DurationUs >= RegistryPerfLogThresholdUs)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Perf][Registry] Compact duration=%.1fus before=%d removed=%d after=%d"), DurationUs, BeforeCount, RemovedCount, RegisteredTargets.Num());
	}
	else
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Perf][Registry] Compact duration=%.1fus before=%d removed=%d after=%d"), DurationUs, BeforeCount, RemovedCount, RegisteredTargets.Num());
	}
}
