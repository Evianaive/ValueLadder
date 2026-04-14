#include "Input/ValueLadderTargetRegistry.h"

#include "ValueLadderLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"

namespace
{
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
				OutTarget = Entry.Value.Target;
				const bool bPropertyHandleValid = OutTarget.PropertyHandle.IsValid();
				const bool bIsValidHandle = bPropertyHandleValid && OutTarget.PropertyHandle->IsValidHandle();

				if (!bIsValidHandle)
				{
					UE_LOG(
						LogValueLadder,
						Verbose,
						TEXT("[Registry] Resolve matched widget with invalid property handle. handle=%llu pathIndex=%d %s %s"),
						static_cast<uint64>(Entry.Key),
						WidgetPathIndex,
						*DescribeWidget(RegisteredWidget),
						*DescribeTarget(OutTarget));
				}

				return bIsValidHandle;
			}
		}
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
		const bool bWidgetValid = It.Value().Widget.IsValid();
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
