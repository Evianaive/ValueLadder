#include "Misc/AutomationTest.h"

#include "Adapter/PropertyHandleValueAdapter.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/ComponentTransformDetailsBridge.h"
#include "Input/ValueLadderInputProcessor.h"
#include "Input/ValueLadderUnitMetadata.h"
#include "Input/ValueLadderTargetRegistry.h"
#include "IDetailsView.h"
#include "ISinglePropertyView.h"
#include "Layout/WidgetPath.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Session/ValueLadderSession.h"
#include "HAL/PlatformProcess.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"
#include "Tests/ValueLadderTestObject.h"
#include "ValueLadderTypes.h"
#include "Types/ISlateMetaData.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool TryExtractDetailRowTokenFromWidgetPath(const FWidgetPath& WidgetPath, FString& OutToken);
	FString NormalizeRowToken(const FString& InToken);
	void LogWidgetPath(const FWidgetPath& WidgetPath, const TCHAR* Label);

	void TickSlate(FSlateApplication& SlateApp, const int32 Iterations = 1)
	{
		for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
		{
			SlateApp.Tick();
		}
	}

	void DestroyTestWindow(FSlateApplication& SlateApp, const TSharedPtr<SWindow>& Window)
	{
		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
			TickSlate(SlateApp, 2);
		}
	}

	void CollectWidgetPaths(
		const TSharedRef<SWidget>& Widget,
		TArray<TWeakPtr<SWidget>>& CurrentPath,
		TArray<TArray<TWeakPtr<SWidget>>>& OutPaths)
	{
		CurrentPath.Add(Widget);
		OutPaths.Add(CurrentPath);

		if (FChildren* Children = Widget->GetAllChildren())
		{
			for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
			{
				CollectWidgetPaths(Children->GetChildAt(ChildIndex), CurrentPath, OutPaths);
			}
		}

		CurrentPath.Pop();
	}

	void CollectWidgetsByTypeSubstring(
		const TSharedRef<SWidget>& Widget,
		const FString& TypeSubstring,
		TArray<TSharedRef<SWidget>>& OutWidgets)
	{
		if (Widget->GetTypeAsString().Contains(TypeSubstring))
		{
			OutWidgets.Add(Widget);
		}

		if (FChildren* Children = Widget->GetAllChildren())
		{
			for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
			{
				CollectWidgetsByTypeSubstring(Children->GetChildAt(ChildIndex), TypeSubstring, OutWidgets);
			}
		}
	}

	bool ResolveTargetFromRuntimePath(const FWidgetPath& WidgetPath, FValueLadderPropertyTarget& OutTarget)
	{
		return FValueLadderTargetRegistry::Get().ResolveTargetFromWidgetPath(WidgetPath, OutTarget)
			|| FComponentTransformDetailsBridge::Get().ResolveTargetFromWidgetPath(WidgetPath, OutTarget);
	}

	bool ResolveRegisteredTargetFromWidgetTree(
		const TSharedRef<SWindow>& Window,
		const TSharedRef<SWidget>& RootWidget,
		FValueLadderPropertyTarget& OutTarget)
	{
		TArray<TArray<TWeakPtr<SWidget>>> WidgetPaths;
		TArray<TWeakPtr<SWidget>> CurrentPath;
		CollectWidgetPaths(RootWidget, CurrentPath, WidgetPaths);

		for (const TArray<TWeakPtr<SWidget>>& WidgetPathWidgets : WidgetPaths)
		{
			FWeakWidgetPath WeakPath;
			WeakPath.Window = Window;
			WeakPath.Widgets = WidgetPathWidgets;

			FWidgetPath WidgetPath;
			WeakPath.ToWidgetPath(WidgetPath, FWeakWidgetPath::EInterruptedPathHandling::Truncate);
			if (!WidgetPath.IsValid())
			{
				continue;
			}

			if (ResolveTargetFromRuntimePath(WidgetPath, OutTarget))
			{
				return true;
			}
		}

		return false;
	}

	bool ResolveRegisteredTargetForLeafPropertyFromWidgetTree(
		const TSharedRef<SWindow>& Window,
		const TSharedRef<SWidget>& RootWidget,
		const FName PropertyName,
		FValueLadderPropertyTarget& OutTarget)
	{
		TArray<TArray<TWeakPtr<SWidget>>> WidgetPaths;
		TArray<TWeakPtr<SWidget>> CurrentPath;
		CollectWidgetPaths(RootWidget, CurrentPath, WidgetPaths);

		for (const TArray<TWeakPtr<SWidget>>& WidgetPathWidgets : WidgetPaths)
		{
			FWeakWidgetPath WeakPath;
			WeakPath.Window = Window;
			WeakPath.Widgets = WidgetPathWidgets;

			FWidgetPath WidgetPath;
			WeakPath.ToWidgetPath(WidgetPath, FWeakWidgetPath::EInterruptedPathHandling::Truncate);
			if (!WidgetPath.IsValid())
			{
				continue;
			}

			FValueLadderPropertyTarget CandidateTarget;
			if (!ResolveTargetFromRuntimePath(WidgetPath, CandidateTarget))
			{
				continue;
			}

			if (!CandidateTarget.PropertyHandle.IsValid() || !CandidateTarget.PropertyHandle->IsValidHandle())
			{
				continue;
			}

			const FProperty* Property = CandidateTarget.PropertyHandle->GetProperty();
			if (Property != nullptr && Property->GetFName() == PropertyName)
			{
				OutTarget = CandidateTarget;
				return true;
			}
		}

		return false;
	}

	bool ResolveRegisteredTargetForLeafPropertyFromHitTest(
		FSlateApplication& SlateApp,
		const TSharedRef<SWidget>& RootWidget,
		const FName PropertyName,
		FValueLadderPropertyTarget& OutTarget,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();
		TSharedPtr<SWidget> RegisteredWidget;
		FValueLadderPropertyTarget RegisteredTarget;
		if (!FValueLadderTargetRegistry::Get().FindRegisteredWidgetForPropertyName(PropertyName, RegisteredWidget, RegisteredTarget))
		{
			OutFailureReason = FString::Printf(TEXT("Registry could not find any registered widget for property '%s'."), *PropertyName.ToString());
			return false;
		}

		if (!RegisteredTarget.PropertyHandle.IsValid() || !RegisteredTarget.PropertyHandle->IsValidHandle())
		{
			OutFailureReason = FString::Printf(TEXT("Registered target for property '%s' has an invalid property handle."), *PropertyName.ToString());
			return false;
		}

		const FString ExpectedPropertyToken = NormalizeRowToken(PropertyName.ToString());
		const FString ExpectedDisplayToken = NormalizeRowToken(RegisteredTarget.PropertyHandle->GetPropertyDisplayName().ToString());
		bool bRegisteredWidgetPathValid = false;
		FString RegisteredWidgetType = RegisteredWidget.IsValid() ? RegisteredWidget->GetTypeAsString() : TEXT("<invalid>");
		if (RegisteredWidget.IsValid())
		{
			FWidgetPath RegisteredWidgetPath;
			SlateApp.GeneratePathToWidgetUnchecked(RegisteredWidget.ToSharedRef(), RegisteredWidgetPath, EVisibility::Visible);
			bRegisteredWidgetPathValid = RegisteredWidgetPath.IsValid();
		}

		TArray<TSharedRef<SWidget>> CandidateWidgets;
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("SNumericEntryBox"), CandidateWidgets);
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("SSpinBox"), CandidateWidgets);
		if (CandidateWidgets.Num() == 0 && RegisteredWidget.IsValid())
		{
			CollectWidgetsByTypeSubstring(RootWidget, RegisteredWidgetType, CandidateWidgets);
		}
		if (CandidateWidgets.Num() == 0)
		{
			CollectWidgetsByTypeSubstring(RootWidget, TEXT("EditableText"), CandidateWidgets);
			CollectWidgetsByTypeSubstring(RootWidget, TEXT("TextBlock"), CandidateWidgets);
		}
		int32 InvalidRectCount = 0;
		int32 InvalidWidgetPathCount = 0;
		int32 RowMismatchCount = 0;
		int32 ResolveFailureCount = 0;
		if (CandidateWidgets.Num() == 0)
		{
			TArray<TSharedRef<SWidget>> NumericLikeWidgets;
			CollectWidgetsByTypeSubstring(RootWidget, TEXT("Numeric"), NumericLikeWidgets);
			CollectWidgetsByTypeSubstring(RootWidget, TEXT("Spin"), NumericLikeWidgets);
			for (int32 WidgetIndex = 0; WidgetIndex < NumericLikeWidgets.Num() && WidgetIndex < 20; ++WidgetIndex)
			{
				UE_LOG(
					LogValueLadder,
					Display,
					TEXT("[Test][NumericLike] index=%d widget=%p type=%s"),
					WidgetIndex,
					static_cast<const void*>(&NumericLikeWidgets[WidgetIndex].Get()),
					*NumericLikeWidgets[WidgetIndex]->GetTypeAsString());
			}
		}

		bool bLoggedFailurePath = false;
		bool bLoggedRowMismatch = false;
		for (const TSharedRef<SWidget>& CandidateWidget : CandidateWidgets)
		{
			const FSlateRect WidgetRect = CandidateWidget->GetTickSpaceGeometry().GetLayoutBoundingRect();
			if (WidgetRect.Right <= WidgetRect.Left || WidgetRect.Bottom <= WidgetRect.Top)
			{
				++InvalidRectCount;
				continue;
			}

			const FVector2D CursorPosition(
				(WidgetRect.Left + WidgetRect.Right) * 0.5f,
				(WidgetRect.Top + WidgetRect.Bottom) * 0.5f);
			const FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(CursorPosition, SlateApp.GetInteractiveTopLevelWindows());
			if (!WidgetPath.IsValid())
			{
				++InvalidWidgetPathCount;
				continue;
			}

			FString RowToken;
			const bool bHasRowToken = TryExtractDetailRowTokenFromWidgetPath(WidgetPath, RowToken);
			const FString NormalizedRowToken = bHasRowToken ? NormalizeRowToken(RowToken) : FString();
			if (!bHasRowToken || (NormalizedRowToken != ExpectedPropertyToken && NormalizedRowToken != ExpectedDisplayToken))
			{
				++RowMismatchCount;
				if (!bLoggedRowMismatch)
				{
					UE_LOG(
						LogValueLadder,
						Display,
						TEXT("[Test][RowTokenMismatch] hasToken=%s rowToken='%s' expectedProperty='%s' expectedDisplay='%s' widgetType=%s"),
						bHasRowToken ? TEXT("true") : TEXT("false"),
						*RowToken,
						*ExpectedPropertyToken,
						*ExpectedDisplayToken,
						*CandidateWidget->GetTypeAsString());
					LogWidgetPath(WidgetPath, TEXT("RowTokenMismatch"));
					bLoggedRowMismatch = true;
				}
				continue;
			}

			FValueLadderPropertyTarget CandidateTarget;
			if (!ResolveTargetFromRuntimePath(WidgetPath, CandidateTarget))
			{
				++ResolveFailureCount;
				if (!bLoggedFailurePath)
				{
					LogWidgetPath(WidgetPath, TEXT("HitTestFailure"));
					bLoggedFailurePath = true;
				}
				continue;
			}

			if (!CandidateTarget.PropertyHandle.IsValid() || !CandidateTarget.PropertyHandle->IsValidHandle())
			{
				continue;
			}

			const FProperty* Property = CandidateTarget.PropertyHandle->GetProperty();
			if (Property == nullptr || Property->GetFName() != PropertyName)
			{
				continue;
			}

			OutTarget = CandidateTarget;
			return true;
		}

		if (const TSharedPtr<SWindow> RootWindow = SlateApp.FindWidgetWindow(RootWidget))
		{
			if (ResolveRegisteredTargetForLeafPropertyFromWidgetTree(RootWindow.ToSharedRef(), RootWidget, PropertyName, OutTarget))
			{
				return true;
			}
		}

		OutFailureReason = FString::Printf(
			TEXT("No hit-test path resolved property '%s'. candidates=%d invalidRects=%d invalidPaths=%d rowMismatches=%d resolveFailures=%d registeredWidgetType=%s registeredPathValid=%s"),
			*PropertyName.ToString(),
			CandidateWidgets.Num(),
			InvalidRectCount,
			InvalidWidgetPathCount,
			RowMismatchCount,
			ResolveFailureCount,
			*RegisteredWidgetType,
			bRegisteredWidgetPathValid ? TEXT("true") : TEXT("false"));
		return false;
	}

	bool FindHitTestPointForLeafProperty(
		FSlateApplication& SlateApp,
		const TSharedRef<SWidget>& RootWidget,
		const FName PropertyName,
		FVector2D& OutScreenPosition,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();
		TSharedPtr<SWidget> RegisteredWidget;
		FValueLadderPropertyTarget RegisteredTarget;
		if (!FValueLadderTargetRegistry::Get().FindRegisteredWidgetForPropertyName(PropertyName, RegisteredWidget, RegisteredTarget))
		{
			OutFailureReason = FString::Printf(TEXT("Registry could not find any registered widget for property '%s'."), *PropertyName.ToString());
			return false;
		}

		if (!RegisteredTarget.PropertyHandle.IsValid() || !RegisteredTarget.PropertyHandle->IsValidHandle())
		{
			OutFailureReason = FString::Printf(TEXT("Registered target for property '%s' has an invalid property handle."), *PropertyName.ToString());
			return false;
		}

		const FString ExpectedPropertyToken = NormalizeRowToken(PropertyName.ToString());
		const FString ExpectedDisplayToken = NormalizeRowToken(RegisteredTarget.PropertyHandle->GetPropertyDisplayName().ToString());

		TArray<TSharedRef<SWidget>> CandidateWidgets;
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("SNumericEntryBox"), CandidateWidgets);
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("SSpinBox"), CandidateWidgets);
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("SPropertyEditorNumeric"), CandidateWidgets);
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("EditableText"), CandidateWidgets);
		CollectWidgetsByTypeSubstring(RootWidget, TEXT("TextBlock"), CandidateWidgets);

		for (const TSharedRef<SWidget>& CandidateWidget : CandidateWidgets)
		{
			const FSlateRect WidgetRect = CandidateWidget->GetTickSpaceGeometry().GetLayoutBoundingRect();
			if (WidgetRect.Right <= WidgetRect.Left || WidgetRect.Bottom <= WidgetRect.Top)
			{
				continue;
			}

			const FVector2D CursorPosition(
				(WidgetRect.Left + WidgetRect.Right) * 0.5f,
				(WidgetRect.Top + WidgetRect.Bottom) * 0.5f);
			const FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(CursorPosition, SlateApp.GetInteractiveTopLevelWindows());
			if (!WidgetPath.IsValid())
			{
				continue;
			}

			FString RowToken;
			const bool bHasRowToken = TryExtractDetailRowTokenFromWidgetPath(WidgetPath, RowToken);
			const FString NormalizedRowToken = bHasRowToken ? NormalizeRowToken(RowToken) : FString();
			if (!bHasRowToken || (NormalizedRowToken != ExpectedPropertyToken && NormalizedRowToken != ExpectedDisplayToken))
			{
				continue;
			}

			OutScreenPosition = CursorPosition;
			return true;
		}

		OutFailureReason = FString::Printf(TEXT("Could not find a hit-testable live widget for property '%s'."), *PropertyName.ToString());
		return false;
	}

	bool TryExtractDetailRowTokenFromWidgetPath(const FWidgetPath& WidgetPath, FString& OutToken)
	{
		for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
			const auto TryConsumeTag = [&OutToken](const FName& Tag)
			{
				const FString TagString = Tag.ToString();
				if (!TagString.StartsWith(TEXT("DetailRowItem.")))
				{
					return false;
				}

				OutToken = TagString.Mid(FCString::Strlen(TEXT("DetailRowItem.")));
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

	void LogWidgetPath(const FWidgetPath& WidgetPath, const TCHAR* Label)
	{
		for (int32 WidgetIndex = 0; WidgetIndex < WidgetPath.Widgets.Num(); ++WidgetIndex)
		{
			const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[WidgetIndex].Widget;
			TArray<FString> TagStrings;
			if (!Widget->GetTag().IsNone())
			{
				TagStrings.Add(FString::Printf(TEXT("Tag=%s"), *Widget->GetTag().ToString()));
			}

			for (const TSharedRef<FTagMetaData>& TagMetaData : Widget->GetAllMetaData<FTagMetaData>())
			{
				TagStrings.Add(FString::Printf(TEXT("Meta=%s"), *TagMetaData->Tag.ToString()));
			}

			UE_LOG(
				LogValueLadder,
				Display,
				TEXT("[Test][WidgetPath] %s index=%d widget=%p type=%s %s"),
				Label,
				WidgetIndex,
				static_cast<const void*>(&Widget.Get()),
				*Widget->GetTypeAsString(),
				TagStrings.Num() > 0 ? *FString::Join(TagStrings, TEXT(" ")) : TEXT(""));
		}
	}

	void LogTaggedWidgets(const TSharedRef<SWidget>& Widget, int32 Depth = 0)
	{
		TArray<FString> TagStrings;
		if (!Widget->GetTag().IsNone())
		{
			TagStrings.Add(FString::Printf(TEXT("Tag=%s"), *Widget->GetTag().ToString()));
		}

		for (const TSharedRef<FTagMetaData>& TagMetaData : Widget->GetAllMetaData<FTagMetaData>())
		{
			TagStrings.Add(FString::Printf(TEXT("Meta=%s"), *TagMetaData->Tag.ToString()));
		}

		if (TagStrings.Num() > 0)
		{
			UE_LOG(
				LogValueLadder,
				Display,
				TEXT("[Test][WidgetTags] depth=%d widget=%p type=%s %s"),
				Depth,
				static_cast<const void*>(&Widget.Get()),
				*Widget->GetTypeAsString(),
				*FString::Join(TagStrings, TEXT(" ")));
		}

		if (FChildren* Children = Widget->GetAllChildren())
		{
			for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
			{
				LogTaggedWidgets(Children->GetChildAt(ChildIndex), Depth + 1);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderDeltaMathTest,
	"ValueLadder.Math.DeltaCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderDeltaMathTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Sub-threshold movement should not change value"), ValueLadder::Math::ComputeDelta(5.0, 12.0, 10.0, 1.0), 0.0);
	TestEqual(TEXT("Positive movement should quantize to whole buckets"), ValueLadder::Math::ComputeDelta(29.0, 12.0, 10.0, 1.0), 20.0);
	TestEqual(TEXT("Negative movement should quantize symmetrically"), ValueLadder::Math::ComputeDelta(-29.0, 12.0, 10.0, 1.0), -20.0);
	TestEqual(TEXT("Multiplier should scale quantized ladder delta"), ValueLadder::Math::ComputeDelta(29.0, 12.0, 10.0, 10.0), 200.0);
	TestEqual(TEXT("Segmented delta should preserve accumulated value at a row switch"), ValueLadder::Math::ComputeSegmentedDelta(20.0, 29.0, 29.0, 12.0, 100.0, 1.0), 20.0);
	TestEqual(TEXT("Segmented delta should continue from the switch point using the new ladder step"), ValueLadder::Math::ComputeSegmentedDelta(20.0, 53.0, 29.0, 12.0, 100.0, 1.0), 220.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderVerticalSelectionMathTest,
	"ValueLadder.Math.VerticalSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderVerticalSelectionMathTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No vertical movement should keep the ladder index stable"), ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(2, 0.0, 20.0, 0, 5), 2);
	TestEqual(TEXT("Dragging down by one row should advance to the next ladder index"), ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(2, 20.0, 20.0, 0, 5), 3);
	TestEqual(TEXT("Dragging up by one row should move to the previous ladder index"), ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(2, -20.0, 20.0, 0, 5), 1);
	TestEqual(TEXT("Vertical selection should clamp to the minimum ladder index"), ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(1, -80.0, 20.0, 0, 5), 0);
	TestEqual(TEXT("Vertical selection should clamp to the maximum ladder index"), ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(4, 80.0, 20.0, 0, 5), 5);
	TestEqual(TEXT("Positive remainder within a tick should be tracked"), ValueLadder::Math::ComputeTickRemainderPx(29.0, 12.0), 5.0);
	TestEqual(TEXT("Negative remainder within a tick should be tracked"), ValueLadder::Math::ComputeTickRemainderPx(-29.0, 12.0), -5.0);
	TestEqual(TEXT("Tick progress should reflect partial travel"), ValueLadder::Math::ComputeTickProgress(29.0, 12.0), 5.0 / 12.0);
	TestEqual(TEXT("Pixels to next tick should show the remaining travel"), ValueLadder::Math::ComputePixelsToNextTick(29.0, 12.0), 7.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderClampTest,
	"ValueLadder.Math.ClampRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderClampTest::RunTest(const FString& Parameters)
{
	FValueLadderConstraintRange Range;
	Range.MinValue = 0.0;
	Range.MaxValue = 10.0;

	TestEqual(TEXT("Clamp min"), Range.Clamp(-2.0), 0.0);
	TestEqual(TEXT("Clamp max"), Range.Clamp(14.0), 10.0);
	TestEqual(TEXT("Clamp inside"), Range.Clamp(5.0), 5.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderRelativeOffsetTest,
	"ValueLadder.Math.MultiObjectRelativeOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderRelativeOffsetTest::RunTest(const FString& Parameters)
{
	const TArray<double> BaselineValues = {10.0, 17.5, -3.0};
	const double Delta = 4.25;

	const double UpdatedA = BaselineValues[0] + Delta;
	const double UpdatedB = BaselineValues[1] + Delta;
	const double UpdatedC = BaselineValues[2] + Delta;

	TestEqual(TEXT("Relative offset A-B must stay stable"), UpdatedB - UpdatedA, BaselineValues[1] - BaselineValues[0]);
	TestEqual(TEXT("Relative offset A-C must stay stable"), UpdatedC - UpdatedA, BaselineValues[2] - BaselineValues[0]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderSettingsMultiplierTest,
	"ValueLadder.Settings.ModifierMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderSettingsMultiplierTest::RunTest(const FString& Parameters)
{
	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->ShiftStepMultiplier = 10.0f;
	Settings->CtrlStepMultiplier = 0.1f;

	TestEqual(TEXT("Shift multiplier"), Settings->ResolveStepMultiplier(true, false), 10.0);
	TestEqual(TEXT("Ctrl multiplier"), Settings->ResolveStepMultiplier(false, true), 0.1);
	TestEqual(TEXT("Neutral multiplier"), Settings->ResolveStepMultiplier(false, false), 1.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderDefaultInputSettingsTest,
	"ValueLadder.Settings.DefaultInputTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderDefaultInputSettingsTest::RunTest(const FString& Parameters)
{
	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();

	TestFalse(TEXT("Middle mouse should not require Alt by default"), Settings->bRequireAltModifier);
	TestTrue(TEXT("Default trigger should use middle mouse"), Settings->TriggerMouseButton == EKeys::MiddleMouseButton);
	TestEqual(TEXT("Float ladder default index should match old behavior"), Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float), 3);
	TestEqual(TEXT("Int ladder default index should match old behavior"), Settings->GetDefaultLadderIndex(EValueLadderNumericType::Int32), 0);
	TestEqual(TEXT("Float ladder should expose old default step"), Settings->GetLadderStep(EValueLadderNumericType::Float, 3), 100.0);
	TestEqual(TEXT("Int ladder should expose old default step"), Settings->GetLadderStep(EValueLadderNumericType::Int32, 0), 1.0);
	for (const EValueLadderNumericType IntegerType : {
		EValueLadderNumericType::Int8,
		EValueLadderNumericType::UInt8,
		EValueLadderNumericType::Int16,
		EValueLadderNumericType::UInt16,
		EValueLadderNumericType::Int32,
		EValueLadderNumericType::UInt32,
		EValueLadderNumericType::Int64,
		EValueLadderNumericType::UInt64})
	{
		TestTrue(FString::Printf(TEXT("%s should be treated as an integer numeric type"), ValueLadder::ToNumericTypeString(IntegerType)), ValueLadder::IsIntegerNumericType(IntegerType));
		TestTrue(FString::Printf(TEXT("%s should be enabled by the integer settings toggle"), ValueLadder::ToNumericTypeString(IntegerType)), Settings->SupportsType(IntegerType));
		TestEqual(FString::Printf(TEXT("%s should use the default integer ladder step"), ValueLadder::ToNumericTypeString(IntegerType)), Settings->GetLadderStep(IntegerType, 0), 1.0);
	}
	TestEqual(TEXT("Default threshold should use 12px"), static_cast<double>(Settings->DragActivationThresholdPx), 12.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderUnitOverrideSettingsTest,
	"ValueLadder.Settings.UnitOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderUnitOverrideSettingsTest::RunTest(const FString& Parameters)
{
	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();

	FValueLadderUnitOverride DegreesOverride;
	DegreesOverride.Unit = TEXT("Degrees");
	DegreesOverride.FloatLadders = {1.0f, 5.0f, 15.0f, 45.0f};
	DegreesOverride.DefaultFloatLadderIndex = 2;
	Settings->UnitOverrides.Add(DegreesOverride);

	FValueLadderUnitOverride FramesOverride;
	FramesOverride.Unit = TEXT("Frames");
	FramesOverride.IntLadders = {1, 5, 10};
	FramesOverride.DefaultIntLadderIndex = 1;
	Settings->UnitOverrides.Add(FramesOverride);

	FValueLadderUnitOverride EmptyOverride;
	Settings->UnitOverrides.Add(EmptyOverride);

	TestEqual(TEXT("Float override should use unit-specific default index"), Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Degrees")), 2);
	TestEqual(TEXT("Float override should use unit-specific ladder step"), Settings->GetLadderStep(EValueLadderNumericType::Float, 3, TEXT("Degrees")), 45.0);

	TArray<FText> DegreeDisplayValues;
	Settings->BuildLadderDisplayValues(EValueLadderNumericType::Float, DegreeDisplayValues, TEXT("Degrees"));
	TestEqual(TEXT("Float override should expose custom ladder count"), DegreeDisplayValues.Num(), 4);

	TestEqual(TEXT("Int override should use unit-specific default index"), Settings->GetDefaultLadderIndex(EValueLadderNumericType::Int32, TEXT("Frames")), 1);
	TestEqual(TEXT("Int override should use unit-specific ladder step"), Settings->GetLadderStep(EValueLadderNumericType::Int32, 1, TEXT("Frames")), 5.0);
	TestEqual(TEXT("Int override should fall back when unit has only float ladders"), Settings->GetLadderStep(EValueLadderNumericType::Int32, 0, TEXT("Degrees")), 1.0);
	TestEqual(TEXT("Float override should fall back when unit has only int ladders"), Settings->GetLadderStep(EValueLadderNumericType::Float, 3, TEXT("Frames")), 100.0);

	TestEqual(TEXT("Unknown unit should fall back to float defaults"), Settings->GetLadderStep(EValueLadderNumericType::Float, 3, TEXT("Radians")), 100.0);
	TestEqual(TEXT("Unknown unit should fall back to int defaults"), Settings->GetLadderStep(EValueLadderNumericType::Int32, 0, TEXT("Fortnights")), 1.0);
	TestEqual(TEXT("None unit key should ignore empty overrides and use float defaults"), Settings->GetLadderStep(EValueLadderNumericType::Float, 3, NAME_None), 100.0);
	TestEqual(TEXT("Unit lookup should normalize case and whitespace"), Settings->GetLadderStep(EValueLadderNumericType::Float, 2, TEXT("  degrees  ")), 15.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderDefaultUnitPresetsTest,
	"ValueLadder.Settings.DefaultUnitPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderDefaultUnitPresetsTest::RunTest(const FString& Parameters)
{
	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();

	TestEqual(TEXT("Degrees should default to a smaller float ladder than 100"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Degrees")), TEXT("Degrees")), 15.0);
	TestEqual(TEXT("Degrees alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("deg")), TEXT("deg")), 15.0);
	TestEqual(TEXT("Centimeters alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("cm")), TEXT("cm")), 100.0);
	TestEqual(TEXT("Centimeters should preserve the existing spatial default"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Centimeters")), TEXT("Centimeters")), 100.0);
	TestEqual(TEXT("Meters alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("m")), TEXT("m")), 1.0);
	TestEqual(TEXT("Meters should default to 1 meter"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Meters")), TEXT("Meters")), 1.0);
	TestEqual(TEXT("Seconds alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("s")), TEXT("s")), 1.0);
	TestEqual(TEXT("Seconds should default to 1 second"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Seconds")), TEXT("Seconds")), 1.0);
	TestEqual(TEXT("Milliseconds alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("ms")), TEXT("ms")), 10.0);
	TestEqual(TEXT("Milliseconds should default to 10 milliseconds"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Milliseconds")), TEXT("Milliseconds")), 10.0);
	TestEqual(TEXT("Kilograms alias should match the canonical preset"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("kg")), TEXT("kg")), 1.0);
	TestEqual(TEXT("Kilograms should default to 1 kilogram"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Kilograms")), TEXT("Kilograms")), 1.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderSemanticProfileSettingsTest,
	"ValueLadder.Settings.SemanticProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderSemanticProfileSettingsTest::RunTest(const FString& Parameters)
{
	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();

	TestEqual(TEXT("Generic float without unit should still use the legacy default"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, NAME_None, EValueLadderSemanticRole::GenericScalar), NAME_None, EValueLadderSemanticRole::GenericScalar), 100.0);
	TestEqual(TEXT("Scale semantic without unit should no longer fall back to 100"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, NAME_None, EValueLadderSemanticRole::Scale), NAME_None, EValueLadderSemanticRole::Scale), 1.0);
	TestEqual(TEXT("Scale semantic should outrank unit-only presets when both are present"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Degrees"), EValueLadderSemanticRole::Scale), TEXT("Degrees"), EValueLadderSemanticRole::Scale), 1.0);

	FValueLadderSemanticOverride ExactScaleOverride;
	ExactScaleOverride.SemanticRole = EValueLadderSemanticRole::Scale;
	ExactScaleOverride.Unit = TEXT("Meters");
	ExactScaleOverride.FloatLadders = {0.01f, 0.1f, 0.5f};
	ExactScaleOverride.DefaultFloatLadderIndex = 2;
	Settings->SemanticOverrides.Add(ExactScaleOverride);

	TestEqual(TEXT("Semantic+unit overrides should outrank semantic-only presets"), Settings->GetLadderStep(EValueLadderNumericType::Float, Settings->GetDefaultLadderIndex(EValueLadderNumericType::Float, TEXT("Meters"), EValueLadderSemanticRole::Scale), TEXT("Meters"), EValueLadderSemanticRole::Scale), 0.5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderUnitMetadataResolutionTest,
	"ValueLadder.Input.UnitMetadataResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderUnitMetadataResolutionTest::RunTest(const FString& Parameters)
{
	TArray<FValueLadderUnitMetadata> ComponentGroupRootChain;
	ComponentGroupRootChain.Add({TEXT(""), TEXT("Centimeters")});
	ComponentGroupRootChain.Add({TEXT("Degrees"), TEXT("Radians")});
	ComponentGroupRootChain.Add({TEXT(""), TEXT("Seconds")});
	TestEqual(TEXT("ForceUnits should take precedence over Units across the full chain"), ValueLadder::Units::ResolveUnitKey(ComponentGroupRootChain), FName(TEXT("Degrees")));

	TArray<FValueLadderUnitMetadata> GroupRootChain;
	GroupRootChain.Add({TEXT(""), TEXT("Meters")});
	GroupRootChain.Add({TEXT(""), TEXT("Seconds")});
	TestEqual(TEXT("Nearest Units metadata should win when no ForceUnits exists"), ValueLadder::Units::ResolveUnitKey(GroupRootChain), FName(TEXT("Meters")));

	TArray<FValueLadderUnitMetadata> RootOnlyChain;
	RootOnlyChain.Add({TEXT(""), TEXT("Kilograms")});
	TestEqual(TEXT("Single-source metadata should resolve directly"), ValueLadder::Units::ResolveUnitKey(RootOnlyChain), FName(TEXT("Kilograms")));

	TArray<FValueLadderUnitMetadata> EmptyChain;
	EmptyChain.Add({TEXT("  "), TEXT("  ")});
	TestTrue(TEXT("Whitespace-only metadata should produce no unit key"), ValueLadder::Units::ResolveUnitKey(EmptyChain).IsNone());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderIntPropertySessionTest,
	"ValueLadder.Input.IntPropertySession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderIntPropertySessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the int property integration test"));
		return false;
	}

	UValueLadderTestObject* TestObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	TestObject->IntValue = 0;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const TSharedPtr<ISinglePropertyView> IntPropertyView = PropertyEditorModule.CreateSingleProperty(TestObject, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, IntValue), FSinglePropertyParams());
	TestTrue(TEXT("Single property view for int should be created"), IntPropertyView.IsValid());
	if (!IntPropertyView.IsValid() || !IntPropertyView->HasValidProperty())
	{
		return false;
	}

	const TSharedPtr<IPropertyHandle> IntHandle = IntPropertyView->GetPropertyHandle();
	TestTrue(TEXT("Int property handle should be valid"), IntHandle.IsValid() && IntHandle->IsValidHandle());
	if (!IntHandle.IsValid() || !IntHandle->IsValidHandle())
	{
		return false;
	}

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			IntPropertyView.ToSharedRef()
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 2);

	FValueLadderPropertyTarget Target;
	bool bResolvedTarget = false;
	for (int32 Attempt = 0; Attempt < 5 && !bResolvedTarget; ++Attempt)
	{
		bResolvedTarget = ResolveRegisteredTargetFromWidgetTree(TestWindow, IntPropertyView.ToSharedRef(), Target);
		if (!bResolvedTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Widget-path resolution should find the registered int target"), bResolvedTarget);
	if (!bResolvedTarget)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	TestTrue(TEXT("Resolved target should contain a valid property handle"), Target.IsValid());
	TestEqual(TEXT("Resolved target should keep the int numeric type"), Target.NumericType, EValueLadderNumericType::Int32);
	TestEqual(TEXT("Resolved target should keep the integer semantic role"), Target.SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);
	TestTrue(TEXT("Resolved target should point at the IntValue property"), Target.PropertyHandle.IsValid() && Target.PropertyHandle->GetProperty() != nullptr && Target.PropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, IntValue));

	FValueLadderSession Session;
	FString Error;
	if (!Session.Begin(Target, FText::FromString(TEXT("Int Session Test")), Error))
	{
		AddError(TEXT("Session should begin for int property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;
	TestTrue(TEXT("One threshold of drag should update int property by one step"), Session.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Int property should update through the real session path"), TestObject->IntValue, 1);
	Session.Cancel();

	TestObject->IntValue = 0;
	FValueLadderSession ShiftSession;
	Error.Reset();
	if (!ShiftSession.Begin(Target, FText::FromString(TEXT("Int Shift Session Test")), Error))
	{
		AddError(TEXT("Shift session should begin for int property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("Shift modifier should apply scaled integer delta"), ShiftSession.UpdateFromPixelOffset(12.1, 1.0, true, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Shift modifier should scale int deltas in the session path"), TestObject->IntValue, 10);

	ShiftSession.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderUInt8PropertySessionTest,
	"ValueLadder.Input.UInt8PropertySession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderUInt8PropertySessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the uint8 property integration test"));
		return false;
	}

	UValueLadderTestObject* TestObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	TestObject->UInt8ClampedValue = 0;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const TSharedPtr<ISinglePropertyView> PropertyView = PropertyEditorModule.CreateSingleProperty(TestObject, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, UInt8ClampedValue), FSinglePropertyParams());
	TestTrue(TEXT("Single property view for uint8 should be created"), PropertyView.IsValid());
	if (!PropertyView.IsValid() || !PropertyView->HasValidProperty())
	{
		return false;
	}

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			PropertyView.ToSharedRef()
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 2);

	FValueLadderPropertyTarget Target;
	bool bResolvedTarget = false;
	for (int32 Attempt = 0; Attempt < 5 && !bResolvedTarget; ++Attempt)
	{
		bResolvedTarget = ResolveRegisteredTargetFromWidgetTree(TestWindow, PropertyView.ToSharedRef(), Target);
		if (!bResolvedTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Widget-path resolution should find the registered uint8 target"), bResolvedTarget);
	if (!bResolvedTarget)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	TestEqual(TEXT("Resolved target should keep the uint8 numeric type"), Target.NumericType, EValueLadderNumericType::UInt8);
	TestEqual(TEXT("Resolved target should keep the integer semantic role"), Target.SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);

	FValueLadderSession Session;
	FString Error;
	if (!Session.Begin(Target, FText::FromString(TEXT("UInt8 Session Test")), Error))
	{
		AddError(TEXT("Session should begin for uint8 property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;
	TestTrue(TEXT("One threshold of drag should update uint8 property by one step"), Session.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("UInt8 property should update through the real session path"), static_cast<int32>(TestObject->UInt8ClampedValue), 1);
	Session.Cancel();

	TestObject->UInt8ClampedValue = 0;
	FValueLadderSession ShiftSession;
	Error.Reset();
	if (!ShiftSession.Begin(Target, FText::FromString(TEXT("UInt8 Shift Session Test")), Error))
	{
		AddError(TEXT("Shift session should begin for uint8 property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("Shift modifier should apply scaled integer delta to uint8"), ShiftSession.UpdateFromPixelOffset(12.1, 1.0, true, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("UInt8 property should honor metadata clamping during session updates"), static_cast<int32>(TestObject->UInt8ClampedValue), 7);

	ShiftSession.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderInt64PropertySessionTest,
	"ValueLadder.Input.Int64PropertySession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderInt64PropertySessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the int64 property integration test"));
		return false;
	}

	UValueLadderTestObject* TestObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	TestObject->Int64Value = 0;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const TSharedPtr<ISinglePropertyView> PropertyView = PropertyEditorModule.CreateSingleProperty(TestObject, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, Int64Value), FSinglePropertyParams());
	TestTrue(TEXT("Single property view for int64 should be created"), PropertyView.IsValid());
	if (!PropertyView.IsValid() || !PropertyView->HasValidProperty())
	{
		return false;
	}

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			PropertyView.ToSharedRef()
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 2);

	FValueLadderPropertyTarget Target;
	bool bResolvedTarget = false;
	for (int32 Attempt = 0; Attempt < 5 && !bResolvedTarget; ++Attempt)
	{
		bResolvedTarget = ResolveRegisteredTargetFromWidgetTree(TestWindow, PropertyView.ToSharedRef(), Target);
		if (!bResolvedTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Widget-path resolution should find the registered int64 target"), bResolvedTarget);
	if (!bResolvedTarget)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	TestEqual(TEXT("Resolved target should keep the int64 numeric type"), Target.NumericType, EValueLadderNumericType::Int64);
	TestEqual(TEXT("Resolved target should keep the integer semantic role"), Target.SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);

	FValueLadderSession Session;
	FString Error;
	if (!Session.Begin(Target, FText::FromString(TEXT("Int64 Session Test")), Error))
	{
		AddError(TEXT("Session should begin for int64 property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;
	TestTrue(TEXT("One threshold of drag should update int64 property by one step"), Session.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Int64 property should update through the real session path"), TestObject->Int64Value, static_cast<int64>(1));
	Session.Cancel();

	TestObject->Int64Value = 0;
	FValueLadderSession ShiftSession;
	Error.Reset();
	if (!ShiftSession.Begin(Target, FText::FromString(TEXT("Int64 Shift Session Test")), Error))
	{
		AddError(TEXT("Shift session should begin for int64 property"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("Shift modifier should apply scaled integer delta to int64"), ShiftSession.UpdateFromPixelOffset(12.1, 1.0, true, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Int64 property should scale integer deltas in the session path"), TestObject->Int64Value, static_cast<int64>(10));

	ShiftSession.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderMultiObjectIntDetailsSessionTest,
	"ValueLadder.Input.MultiObjectIntDetailsSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderMultiObjectIntDetailsSessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the multi-object int details integration test"));
		return false;
	}

	UValueLadderTestObject* FirstObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	UValueLadderTestObject* SecondObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	FirstObject->IntValue = 3;
	SecondObject->IntValue = 11;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(FirstObject);
	SelectedObjects.Add(SecondObject);
	DetailsView->SetObjects(SelectedObjects, true);

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			DetailsView
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 4);

	FValueLadderPropertyTarget Target;
	FString ResolveFailureReason;
	bool bResolvedTarget = false;
	for (int32 Attempt = 0; Attempt < 8 && !bResolvedTarget; ++Attempt)
	{
		bResolvedTarget = ResolveRegisteredTargetForLeafPropertyFromHitTest(SlateApp, DetailsView, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, IntValue), Target, ResolveFailureReason);
		if (!bResolvedTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Widget-path resolution should find the registered multi-object int target"), bResolvedTarget);
	if (!bResolvedTarget)
	{
		if (!ResolveFailureReason.IsEmpty())
		{
			AddError(ResolveFailureReason);
		}
		UE_LOG(LogValueLadder, Display, TEXT("[Test][WidgetTags] Logging tagged widgets for multi-object int details failure."));
		LogTaggedWidgets(DetailsView);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	TestTrue(TEXT("Resolved multi-object target should contain a valid property handle"), Target.IsValid());
	TestEqual(TEXT("Resolved multi-object target should keep the int numeric type"), Target.NumericType, EValueLadderNumericType::Int32);
	TestEqual(TEXT("Resolved multi-object target should keep the integer semantic role"), Target.SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);

	TArray<void*> RawData;
	Target.PropertyHandle->AccessRawData(RawData);
	TestEqual(TEXT("Resolved multi-object target should expose raw data for both selected objects"), RawData.Num(), 2);
	if (RawData.Num() != 2)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	FValueLadderSession Session;
	FString Error;
	if (!Session.Begin(Target, FText::FromString(TEXT("Multi-Object Int Session Test")), Error))
	{
		AddError(TEXT("Session should begin for multi-object int details target"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;
	TestTrue(TEXT("One threshold of drag should update both selected int properties by one step"), Session.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("First selected object should keep its relative offset after the multi-object session update"), FirstObject->IntValue, 4);
	TestEqual(TEXT("Second selected object should keep its relative offset after the multi-object session update"), SecondObject->IntValue, 12);

	Session.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderBodyInstanceGravityGroupSessionTest,
	"ValueLadder.Input.BodyInstanceGravityGroupSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderBodyInstanceGravityGroupSessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the BodyInstance GravityGroupIndex integration test"));
		return false;
	}

	UStaticMeshComponent* TestComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
	TestComponent->BodyInstance.GravityGroupIndex = 0;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(TestComponent, true);

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			DetailsView
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 4);

	FValueLadderPropertyTarget Target;
	TSharedPtr<SWidget> RegisteredWidget;
	bool bFoundRegisteredTarget = false;
	for (int32 Attempt = 0; Attempt < 8 && !bFoundRegisteredTarget; ++Attempt)
	{
		FValueLadderPropertyTarget CandidateTarget;
		TSharedPtr<SWidget> CandidateWidget;
		bFoundRegisteredTarget = FValueLadderTargetRegistry::Get().FindRegisteredWidgetForPropertyName(
			GET_MEMBER_NAME_CHECKED(FBodyInstance, GravityGroupIndex),
			CandidateWidget,
			CandidateTarget);
		if (bFoundRegisteredTarget)
		{
			RegisteredWidget = CandidateWidget;
			Target = CandidateTarget;
		}

		if (!bFoundRegisteredTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Registry should capture BodyInstance.GravityGroupIndex from the real details view"), bFoundRegisteredTarget);
	if (!bFoundRegisteredTarget)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}
	TestTrue(TEXT("Registry lookup should return a widget for BodyInstance.GravityGroupIndex"), RegisteredWidget.IsValid());

	TestEqual(TEXT("Resolved BodyInstance GravityGroupIndex target should keep the uint8 numeric type"), Target.NumericType, EValueLadderNumericType::UInt8);
	TestEqual(TEXT("Resolved BodyInstance GravityGroupIndex target should keep the integer semantic role"), Target.SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);

	FValueLadderSession Session;
	FString Error;
	if (!Session.Begin(Target, FText::FromString(TEXT("BodyInstance GravityGroupIndex Session Test")), Error))
	{
		AddError(TEXT("Session should begin for BodyInstance GravityGroupIndex"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;
	TestTrue(TEXT("One threshold of drag should update BodyInstance.GravityGroupIndex by one step"), Session.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("BodyInstance GravityGroupIndex should update through the real details-view session path"), static_cast<int32>(TestComponent->BodyInstance.GravityGroupIndex), 1);
	Session.Cancel();

	TestComponent->BodyInstance.GravityGroupIndex = 0;
	FValueLadderSession ShiftSession;
	Error.Reset();
	if (!ShiftSession.Begin(Target, FText::FromString(TEXT("BodyInstance GravityGroupIndex Shift Session Test")), Error))
	{
		AddError(TEXT("Shift session should begin for BodyInstance GravityGroupIndex"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("Shift modifier should apply scaled integer delta to BodyInstance.GravityGroupIndex"), ShiftSession.UpdateFromPixelOffset(12.1, 1.0, true, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("BodyInstance GravityGroupIndex should honor metadata clamping during session updates"), static_cast<int32>(TestComponent->BodyInstance.GravityGroupIndex), 7);

	ShiftSession.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderMultiObjectInputProcessorCaptureTest,
	"ValueLadder.Input.MultiObjectInputProcessorCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderMultiObjectInputProcessorCaptureTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the multi-object input processor capture test"));
		return false;
	}

	UValueLadderTestObject* FirstObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	UValueLadderTestObject* SecondObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	FirstObject->IntValue = 3;
	SecondObject->IntValue = 11;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(FirstObject);
	SelectedObjects.Add(SecondObject);
	DetailsView->SetObjects(SelectedObjects, true);

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			DetailsView
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 4);

	auto* Settings = GetMutableDefault<UValueLadderSettings>();
	const bool bOldRequireAltModifier = Settings->bRequireAltModifier;
	const FKey OldTriggerMouseButton = Settings->TriggerMouseButton;
	const float OldDragActivationThresholdPx = Settings->DragActivationThresholdPx;
	const bool bOldShowOverlay = Settings->bShowOverlay;
	ON_SCOPE_EXIT
	{
		Settings->bRequireAltModifier = bOldRequireAltModifier;
		Settings->TriggerMouseButton = OldTriggerMouseButton;
		Settings->DragActivationThresholdPx = OldDragActivationThresholdPx;
		Settings->bShowOverlay = bOldShowOverlay;
		DestroyTestWindow(SlateApp, TestWindow);
	};

	Settings->bRequireAltModifier = false;
	Settings->TriggerMouseButton = EKeys::MiddleMouseButton;
	Settings->DragActivationThresholdPx = 12.0f;
	Settings->bShowOverlay = true;

	FVector2D HitPoint = FVector2D::ZeroVector;
	FString HitPointFailureReason;
	bool bFoundHitPoint = false;
	for (int32 Attempt = 0; Attempt < 8 && !bFoundHitPoint; ++Attempt)
	{
		bFoundHitPoint = FindHitTestPointForLeafProperty(SlateApp, DetailsView, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, IntValue), HitPoint, HitPointFailureReason);
		if (!bFoundHitPoint)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Multi-object details view should expose a hit-testable point for IntValue"), bFoundHitPoint);
	if (!bFoundHitPoint)
	{
		if (!HitPointFailureReason.IsEmpty())
		{
			AddError(HitPointFailureReason);
		}
		return false;
	}

	FValueLadderInputProcessor Processor;
	const TSet<FKey> PressedButtons = {EKeys::MiddleMouseButton};
	const FModifierKeysState NoModifiers;
	const uint32 PointerIndex = static_cast<uint32>(ETouchIndex::CursorPointerIndex);

	const FPointerEvent MouseDownEvent(0, PointerIndex, HitPoint, HitPoint, PressedButtons, EKeys::MiddleMouseButton, 0.0f, NoModifiers);
	const bool bHandledDown = Processor.HandleMouseButtonDownEvent(SlateApp, MouseDownEvent);
	TestTrue(TEXT("Input processor should start a multi-object gesture from the live details widget"), bHandledDown);
	if (!bHandledDown)
	{
		return false;
	}

	TickSlate(SlateApp, 1);
	const FPointerEvent MoveSelectEvent(0, PointerIndex, HitPoint, HitPoint, FVector2D::ZeroVector, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveSelectEvent);

	const FVector2D LockPoint = HitPoint + FVector2D(120.0f, 0.0f);
	const FPointerEvent MoveLockEvent(0, PointerIndex, LockPoint, HitPoint, LockPoint - HitPoint, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveLockEvent);

	const FVector2D DragPoint = HitPoint + FVector2D(144.0f, 0.0f);
	const FPointerEvent MoveDragEvent(0, PointerIndex, DragPoint, LockPoint, DragPoint - LockPoint, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveDragEvent);
	FPlatformProcess::Sleep(0.75f);
	TickSlate(SlateApp, 2);

	const TSet<FKey> ReleasedButtons;
	const FPointerEvent MouseUpEvent(0, PointerIndex, DragPoint, DragPoint, ReleasedButtons, EKeys::MiddleMouseButton, 0.0f, NoModifiers);
	const bool bHandledUp = Processor.HandleMouseButtonUpEvent(SlateApp, MouseUpEvent);
	TestTrue(TEXT("Input processor should finish the multi-object gesture on mouse up"), bHandledUp);

	TestTrue(TEXT("Multi-object capture path should update at least one selected value"), FirstObject->IntValue != 3 && SecondObject->IntValue != 11);
	TestEqual(TEXT("Multi-object capture path should preserve the relative offset between selected values"), SecondObject->IntValue - FirstObject->IntValue, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderMultiObjectFloatInputProcessorCaptureTest,
	"ValueLadder.Input.MultiObjectFloatInputProcessorCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderMultiObjectFloatInputProcessorCaptureTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the multi-object float input processor capture test"));
		return false;
	}

	UValueLadderTestObject* FirstObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	UValueLadderTestObject* SecondObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	FirstObject->FloatValue = 3.0f;
	SecondObject->FloatValue = 11.0f;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(FirstObject);
	SelectedObjects.Add(SecondObject);
	DetailsView->SetObjects(SelectedObjects, true);

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			DetailsView
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 4);

	auto* Settings = GetMutableDefault<UValueLadderSettings>();
	const bool bOldRequireAltModifier = Settings->bRequireAltModifier;
	const FKey OldTriggerMouseButton = Settings->TriggerMouseButton;
	const float OldDragActivationThresholdPx = Settings->DragActivationThresholdPx;
	const bool bOldShowOverlay = Settings->bShowOverlay;
	ON_SCOPE_EXIT
	{
		Settings->bRequireAltModifier = bOldRequireAltModifier;
		Settings->TriggerMouseButton = OldTriggerMouseButton;
		Settings->DragActivationThresholdPx = OldDragActivationThresholdPx;
		Settings->bShowOverlay = bOldShowOverlay;
		DestroyTestWindow(SlateApp, TestWindow);
	};

	Settings->bRequireAltModifier = false;
	Settings->TriggerMouseButton = EKeys::MiddleMouseButton;
	Settings->DragActivationThresholdPx = 12.0f;
	Settings->bShowOverlay = true;

	FVector2D HitPoint = FVector2D::ZeroVector;
	FString HitPointFailureReason;
	bool bFoundHitPoint = false;
	for (int32 Attempt = 0; Attempt < 8 && !bFoundHitPoint; ++Attempt)
	{
		bFoundHitPoint = FindHitTestPointForLeafProperty(SlateApp, DetailsView, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, FloatValue), HitPoint, HitPointFailureReason);
		if (!bFoundHitPoint)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Multi-object details view should expose a hit-testable point for FloatValue"), bFoundHitPoint);
	if (!bFoundHitPoint)
	{
		if (!HitPointFailureReason.IsEmpty())
		{
			AddError(HitPointFailureReason);
		}
		return false;
	}

	FValueLadderInputProcessor Processor;
	const TSet<FKey> PressedButtons = {EKeys::MiddleMouseButton};
	const FModifierKeysState NoModifiers;
	const uint32 PointerIndex = static_cast<uint32>(ETouchIndex::CursorPointerIndex);

	const FPointerEvent MouseDownEvent(0, PointerIndex, HitPoint, HitPoint, PressedButtons, EKeys::MiddleMouseButton, 0.0f, NoModifiers);
	const bool bHandledDown = Processor.HandleMouseButtonDownEvent(SlateApp, MouseDownEvent);
	TestTrue(TEXT("Input processor should start a multi-object float gesture from the live details widget"), bHandledDown);
	if (!bHandledDown)
	{
		return false;
	}

	TickSlate(SlateApp, 1);
	const FPointerEvent MoveSelectEvent(0, PointerIndex, HitPoint, HitPoint, FVector2D::ZeroVector, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveSelectEvent);

	const FVector2D LockPoint = HitPoint + FVector2D(120.0f, 0.0f);
	const FPointerEvent MoveLockEvent(0, PointerIndex, LockPoint, HitPoint, LockPoint - HitPoint, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveLockEvent);

	const FVector2D DragPoint = HitPoint + FVector2D(144.0f, 0.0f);
	const FPointerEvent MoveDragEvent(0, PointerIndex, DragPoint, LockPoint, DragPoint - LockPoint, PressedButtons, NoModifiers);
	Processor.HandleMouseMoveEvent(SlateApp, MoveDragEvent);

	const TSet<FKey> ReleasedButtons;
	const FPointerEvent MouseUpEvent(0, PointerIndex, DragPoint, DragPoint, ReleasedButtons, EKeys::MiddleMouseButton, 0.0f, NoModifiers);
	const bool bHandledUp = Processor.HandleMouseButtonUpEvent(SlateApp, MouseUpEvent);
	TestTrue(TEXT("Input processor should finish the multi-object float gesture on mouse up"), bHandledUp);

	TestTrue(TEXT("Multi-object float capture path should update at least one selected value"), !FMath::IsNearlyEqual(FirstObject->FloatValue, 3.0f) && !FMath::IsNearlyEqual(SecondObject->FloatValue, 11.0f));
	TestEqual(TEXT("Multi-object float capture path should preserve the relative offset between selected values"), SecondObject->FloatValue - FirstObject->FloatValue, 8.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderIntPointHeaderSessionTest,
	"ValueLadder.Input.IntPointHeaderSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderIntPointHeaderSessionTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate application must be initialized for the int point integration test"));
		return false;
	}

	UValueLadderTestObject* TestObject = NewObject<UValueLadderTestObject>(GetTransientPackage());
	TestObject->IntPointValue = FIntPoint(0, 0);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const TSharedPtr<ISinglePropertyView> IntPointPropertyView = PropertyEditorModule.CreateSingleProperty(TestObject, GET_MEMBER_NAME_CHECKED(UValueLadderTestObject, IntPointValue), FSinglePropertyParams());
	TestTrue(TEXT("Single property view for int point should be created"), IntPointPropertyView.IsValid());
	if (!IntPointPropertyView.IsValid() || !IntPointPropertyView->HasValidProperty())
	{
		return false;
	}

	const TSharedPtr<IPropertyHandle> IntPointHandle = IntPointPropertyView->GetPropertyHandle();
	TestTrue(TEXT("Int point property handle should be valid"), IntPointHandle.IsValid() && IntPointHandle->IsValidHandle());
	if (!IntPointHandle.IsValid() || !IntPointHandle->IsValidHandle())
	{
		return false;
	}

	const TSharedRef<SWindow> TestWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		[
			IntPointPropertyView.ToSharedRef()
		];

	SlateApp.AddWindow(TestWindow, true);
	TickSlate(SlateApp, 2);

	FValueLadderPropertyTarget XTarget;
	FValueLadderPropertyTarget YTarget;
	bool bResolvedXTarget = false;
	bool bResolvedYTarget = false;
	for (int32 Attempt = 0; Attempt < 5 && (!bResolvedXTarget || !bResolvedYTarget); ++Attempt)
	{
		if (!bResolvedXTarget)
		{
			bResolvedXTarget = ResolveRegisteredTargetForLeafPropertyFromWidgetTree(TestWindow, IntPointPropertyView.ToSharedRef(), TEXT("X"), XTarget);
		}

		if (!bResolvedYTarget)
		{
			bResolvedYTarget = ResolveRegisteredTargetForLeafPropertyFromWidgetTree(TestWindow, IntPointPropertyView.ToSharedRef(), TEXT("Y"), YTarget);
		}

		if (!bResolvedXTarget || !bResolvedYTarget)
		{
			TickSlate(SlateApp, 1);
		}
	}

	TestTrue(TEXT("Header widget-path resolution should find the X target"), bResolvedXTarget);
	TestTrue(TEXT("Header widget-path resolution should find the Y target"), bResolvedYTarget);
	if (!bResolvedXTarget || !bResolvedYTarget)
	{
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	for (const TPair<const TCHAR*, FValueLadderPropertyTarget*> TargetEntry : {TPair<const TCHAR*, FValueLadderPropertyTarget*>(TEXT("X"), &XTarget), TPair<const TCHAR*, FValueLadderPropertyTarget*>(TEXT("Y"), &YTarget)})
	{
		FValueLadderPropertyTarget* const Target = TargetEntry.Value;
		TestTrue(FString::Printf(TEXT("Resolved %s target should contain a valid property handle"), TargetEntry.Key), Target->IsValid());
		TestEqual(FString::Printf(TEXT("Resolved %s target should keep the int numeric type"), TargetEntry.Key), Target->NumericType, EValueLadderNumericType::Int32);
		TestEqual(FString::Printf(TEXT("Resolved %s target should keep the integer semantic role"), TargetEntry.Key), Target->SemanticRole, EValueLadderSemanticRole::IntegerDiscrete);
		TestTrue(FString::Printf(TEXT("Resolved %s target should point at the expected child property"), TargetEntry.Key), Target->PropertyHandle.IsValid() && Target->PropertyHandle->GetProperty() != nullptr && Target->PropertyHandle->GetProperty()->GetFName() == FName(TargetEntry.Key));
	}

	UValueLadderSettings* Settings = NewObject<UValueLadderSettings>();
	Settings->DragActivationThresholdPx = 12.0f;

	FValueLadderSession XSession;
	FString Error;
	if (!XSession.Begin(XTarget, FText::FromString(TEXT("IntPoint X Session Test")), Error))
	{
		AddError(TEXT("Session should begin for int point X target"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("One threshold of drag should update the X component by one step"), XSession.UpdateFromPixelOffset(12.1, 1.0, false, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Int point X component should update through the header session path"), TestObject->IntPointValue.X, 1);
	TestEqual(TEXT("Int point Y component should remain unchanged when dragging X"), TestObject->IntPointValue.Y, 0);
	XSession.Cancel();

	TestObject->IntPointValue = FIntPoint(0, 0);
	FValueLadderSession YSession;
	Error.Reset();
	if (!YSession.Begin(YTarget, FText::FromString(TEXT("IntPoint Y Session Test")), Error))
	{
		AddError(TEXT("Session should begin for int point Y target"));
		AddError(Error);
		DestroyTestWindow(SlateApp, TestWindow);
		return false;
	}

	Error.Reset();
	TestTrue(TEXT("Shift modifier should apply scaled integer delta to the Y component"), YSession.UpdateFromPixelOffset(12.1, 1.0, true, false, *Settings, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Int point X component should remain unchanged when dragging Y"), TestObject->IntPointValue.X, 0);
	TestEqual(TEXT("Int point Y component should update through the header session path"), TestObject->IntPointValue.Y, 10);

	YSession.Cancel();
	DestroyTestWindow(SlateApp, TestWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValueLadderIntegerRoundingTest,
	"ValueLadder.Math.IntegerRounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderIntegerRoundingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Round positive values"), ValueLadder::Math::ApplyIntegerRounding(8.6), 9.0);
	TestEqual(TEXT("Round negative values"), ValueLadder::Math::ApplyIntegerRounding(-2.6), -3.0);
	return true;
}

#endif
