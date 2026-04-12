#include "ValueLadderSettings.h"

double UValueLadderSettings::ResolveStepMultiplier(const bool bShiftDown, const bool bCtrlDown) const
{
	if (bShiftDown && !bCtrlDown)
	{
		return ShiftStepMultiplier;
	}

	if (bCtrlDown && !bShiftDown)
	{
		return CtrlStepMultiplier;
	}

	return 1.0;
}

double UValueLadderSettings::ComputeDeltaFromPixelOffset(const double PixelOffset, const bool bShiftDown, const bool bCtrlDown) const
{
	return PixelOffset * static_cast<double>(DragSensitivity) * ResolveStepMultiplier(bShiftDown, bCtrlDown);
}

bool UValueLadderSettings::SupportsType(const EValueLadderNumericType NumericType) const
{
	switch (NumericType)
	{
	case EValueLadderNumericType::Float:
		return bEnableFloat;
	case EValueLadderNumericType::Double:
		return bEnableDouble;
	case EValueLadderNumericType::Int32:
		return bEnableInt32;
	default:
		return false;
	}
}

