#include "Misc/AutomationTest.h"

#include "ValueLadderSettings.h"
#include "ValueLadderTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

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
	FValueLadderSelectionColumnGateTest,
	"ValueLadder.Math.SelectionColumnGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValueLadderSelectionColumnGateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Offset inside selection column should report unlocked"), ValueLadder::Math::IsInsideSelectionColumn(40.0, 100.0));
	TestFalse(TEXT("Offset outside selection column should report locked"), ValueLadder::Math::IsInsideSelectionColumn(60.0, 100.0));
	TestEqual(TEXT("Movement inside selection column should not accumulate"), ValueLadder::Math::ApplySelectionColumnGate(40.0, 100.0), 0.0);
	TestEqual(TEXT("Movement should begin accumulating after leaving the positive edge of the column"), ValueLadder::Math::ApplySelectionColumnGate(80.0, 100.0), 30.0);
	TestEqual(TEXT("Movement should begin accumulating after leaving the negative edge of the column"), ValueLadder::Math::ApplySelectionColumnGate(-80.0, 100.0), -30.0);
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
	TestEqual(TEXT("Default threshold should use 12px"), static_cast<double>(Settings->DragActivationThresholdPx), 12.0);

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
