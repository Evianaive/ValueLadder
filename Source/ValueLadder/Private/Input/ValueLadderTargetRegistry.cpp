#include "Input/ValueLadderTargetRegistry.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"

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

	return NewHandle;
}

void FValueLadderTargetRegistry::UnregisterTarget(const FValueLadderTargetHandle Handle)
{
	FScopeLock Lock(&RegistryMutex);
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
				return OutTarget.IsValid();
			}
		}
	}

	return false;
}

void FValueLadderTargetRegistry::Compact_NoLock()
{
	for (auto It = RegisteredTargets.CreateIterator(); It; ++It)
	{
		if (!It.Value().Widget.IsValid() || !It.Value().Target.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
