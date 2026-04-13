#include "Input/ValueLadderInputProcessor.h"

#include "Input/ValueLadderTargetRegistry.h"
#include "UI/SValueLadderOverlay.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ValueLadderInputProcessor"

namespace
{
	constexpr float LadderRowHeightPx = 20.0f;

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

FValueLadderInputProcessor::FValueLadderInputProcessor()
{
}

void FValueLadderInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FValueLadderInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (bDragging)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Input] MouseDown ignored because a gesture is already active. ActiveGestureId=%llu"), ActiveGestureId);
		return true;
	}

	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();
	if (Settings == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseDown rejected: settings default object is null."));
		return false;
	}

	if (MouseEvent.GetEffectingButton() != Settings->TriggerMouseButton)
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseDown ignored: button=%s expected=%s"), *MouseEvent.GetEffectingButton().ToString(), *Settings->TriggerMouseButton.ToString());
		return false;
	}

	if (Settings->bRequireAltModifier && !MouseEvent.IsAltDown())
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseDown rejected: Alt modifier required but not pressed. Trigger=%s"), *Settings->TriggerMouseButton.ToString());
		return false;
	}

	const FVector2D CursorPosition = MouseEvent.GetScreenSpacePosition();
	const FWidgetPath WidgetsUnderCursor = SlateApp.LocateWindowUnderMouse(CursorPosition, SlateApp.GetInteractiveTopLevelWindows());
	UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseDown accepted for trigger=%s at (%.1f, %.1f). WidgetPathDepth=%d ShowOverlay=%s"), *Settings->TriggerMouseButton.ToString(), CursorPosition.X, CursorPosition.Y, WidgetsUnderCursor.Widgets.Num(), Settings->bShowOverlay ? TEXT("true") : TEXT("false"));

	FValueLadderPropertyTarget Target;
	if (!FValueLadderTargetRegistry::Get().ResolveTargetFromWidgetPath(WidgetsUnderCursor, Target))
	{
		UE_LOG(
			LogValueLadder,
			Warning,
			TEXT("[Input] MouseDown rejected: no registered ValueLadder target resolved from widget path. Cursor=(%.1f, %.1f) WidgetPathDepth=%d"),
			CursorPosition.X,
			CursorPosition.Y,
			WidgetsUnderCursor.Widgets.Num());
		return false;
	}

	if (Target.bIsVectorComponent)
	{
		if (!Settings->bEnableVector)
		{
			UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseDown rejected: vector target resolved but vector support is disabled."));
			return false;
		}
	}
	else if (!Settings->SupportsType(Target.NumericType))
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseDown rejected: target type=%s is disabled in settings."), ToNumericTypeString(Target.NumericType));
		return false;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseDown resolved target type=%s vector=%s"), ToNumericTypeString(Target.NumericType), Target.bIsVectorComponent ? TEXT("true") : TEXT("false"));

	return InitializeGesture(*Settings, SlateApp, MouseEvent, Target);
}

bool FValueLadderInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();

	if (!bDragging)
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseUp ignored because no gesture is active."));
		return false;
	}

	if (Settings == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseUp encountered null settings; cancelling gesture %llu."), ActiveGestureId);
		Session.Cancel();
		bDragging = false;
		DestroyOverlay();
		ActiveGestureId = 0;
		return false;
	}

	if (MouseEvent.GetEffectingButton() != Settings->TriggerMouseButton)
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseUp ignored: button=%s expected=%s activeGesture=%llu"), *MouseEvent.GetEffectingButton().ToString(), *Settings->TriggerMouseButton.ToString(), ActiveGestureId);
		return false;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseUp finishing gesture=%llu commit=%s delta=%.6g"), ActiveGestureId, !FMath::IsNearlyZero(Session.GetCurrentDelta()) ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta());
	EndGesture(!FMath::IsNearlyZero(Session.GetCurrentDelta()));
	return true;
}

bool FValueLadderInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();

	if (!bDragging)
	{
		return false;
	}

	if (Settings == nullptr)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove encountered null settings; cancelling gesture %llu."), ActiveGestureId);
		Session.Cancel();
		bDragging = false;
		DestroyOverlay();
		ActiveGestureId = 0;
		return false;
	}

	if (!MouseEvent.IsMouseButtonDown(Settings->TriggerMouseButton))
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseMove observed released trigger button; ending gesture=%llu commit=%s delta=%.6g"), ActiveGestureId, !FMath::IsNearlyZero(Session.GetCurrentDelta()) ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta());
		EndGesture(!FMath::IsNearlyZero(Session.GetCurrentDelta()));
		return true;
	}

	ActiveLadderIndex = ResolveActiveLadderIndex(*Settings, MouseEvent.GetScreenSpacePosition());
	const double LadderStep = Settings->GetLadderStep(ActiveTarget.NumericType, ActiveLadderIndex);

	const double PixelOffset = MouseEvent.GetScreenSpacePosition().X - DragStartPosition.X;
	UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseMove gesture=%llu pixelOffset=%.6g ladderIndex=%d ladderStep=%.6g shift=%s ctrl=%s"), ActiveGestureId, PixelOffset, ActiveLadderIndex, LadderStep, MouseEvent.IsShiftDown() ? TEXT("true") : TEXT("false"), MouseEvent.IsControlDown() ? TEXT("true") : TEXT("false"));
	FString SessionError;
	if (!Session.UpdateFromPixelOffset(PixelOffset, LadderStep, MouseEvent.IsShiftDown(), MouseEvent.IsControlDown(), *Settings, SessionError))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove update failed for gesture=%llu: %s"), ActiveGestureId, *SessionError);
		EndGesture(false);
		return true;
	}

	UpdateOverlay();
	return true;
}

bool FValueLadderInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (!bDragging)
	{
		return false;
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] Escape pressed; cancelling gesture=%llu"), ActiveGestureId);
		EndGesture(false);
		return true;
	}

	return false;
}

bool FValueLadderInputProcessor::InitializeGesture(const UValueLadderSettings& Settings, FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, const FValueLadderPropertyTarget& Target)
{
	const uint64 GestureId = NextGestureId++;
	FString BeginError;
	if (!Session.Begin(Target, LOCTEXT("ValueLadderDrag", "Value Ladder Drag"), BeginError))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] InitializeGesture failed before activation. Gesture=%llu Error=%s"), GestureId, *BeginError);
		return false;
	}

	ActiveGestureId = GestureId;
	bDragging = true;
	ActiveTarget = Target;
	DragStartPosition = MouseEvent.GetScreenSpacePosition();
	Settings.BuildLadderDisplayValues(Target.NumericType, ActiveLadderValues);
	StartLadderIndex = Settings.GetDefaultLadderIndex(Target.NumericType);
	ActiveLadderIndex = StartLadderIndex;
	OverlayAnchorPosition = DragStartPosition - FVector2D(50.0f, 10.0f + static_cast<float>(StartLadderIndex) * LadderRowHeightPx);
	UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu initialized. Type=%s StartIndex=%d LadderCount=%d Anchor=(%.1f, %.1f)"), ActiveGestureId, ToNumericTypeString(Target.NumericType), StartLadderIndex, ActiveLadderValues.Num(), OverlayAnchorPosition.X, OverlayAnchorPosition.Y);

	if (Settings.bShowOverlay)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu requested overlay creation."), ActiveGestureId);
		EnsureOverlay(SlateApp);
		UpdateOverlay();
	}
	else
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu did not create overlay because bShowOverlay=false."), ActiveGestureId);
	}

	return true;
}

void FValueLadderInputProcessor::EnsureOverlay(FSlateApplication& SlateApp)
{
	if (OverlayWindow.IsValid())
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Overlay] Gesture=%llu reused existing overlay window."), ActiveGestureId);
		return;
	}

	OverlayWidget = SNew(SValueLadderOverlay);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		.ScreenPosition(OverlayAnchorPosition)
		[
			OverlayWidget.ToSharedRef()
		];

	SlateApp.AddWindow(Window, true);
	OverlayWindow = Window;
	UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu created overlay window at (%.1f, %.1f)."), ActiveGestureId, OverlayAnchorPosition.X, OverlayAnchorPosition.Y);
}

void FValueLadderInputProcessor::UpdateOverlay()
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->UpdateDisplay(ActiveLadderValues, ActiveLadderIndex, Session.GetCurrentMultiplier(), Session.GetCurrentDelta(), Session.GetPreviewValueText());
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Overlay] Gesture=%llu updated overlay. ActiveIndex=%d Delta=%.6g Multiplier=%.6g"), ActiveGestureId, ActiveLadderIndex, Session.GetCurrentDelta(), Session.GetCurrentMultiplier());
	}
	else
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu update requested but overlay widget is invalid."), ActiveGestureId);
	}
}

void FValueLadderInputProcessor::DestroyOverlay()
{
	if (OverlayWindow.IsValid())
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu destroying overlay window."), ActiveGestureId);
		OverlayWindow.Pin()->RequestDestroyWindow();
		OverlayWindow.Reset();
	}
	else
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Overlay] Gesture=%llu destroy requested but overlay window was already invalid."), ActiveGestureId);
	}

	OverlayWidget.Reset();
}

void FValueLadderInputProcessor::EndGesture(const bool bCommit)
{
	UE_LOG(LogValueLadder, Display, TEXT("[Input] EndGesture gesture=%llu commit=%s delta=%.6g"), ActiveGestureId, bCommit ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta());
	if (bCommit)
	{
		Session.Commit();
	}
	else
	{
		Session.Cancel();
	}

	bDragging = false;
	ActiveTarget = FValueLadderPropertyTarget();
	ActiveLadderValues.Reset();
	StartLadderIndex = 0;
	ActiveLadderIndex = 0;
	DestroyOverlay();
	ActiveGestureId = 0;
}

int32 FValueLadderInputProcessor::ResolveActiveLadderIndex(const UValueLadderSettings& Settings, const FVector2D& CursorPosition) const
{
	const double VerticalOffset = CursorPosition.Y - DragStartPosition.Y;
	const int32 LadderIndexOffset = FMath::RoundToInt(VerticalOffset / LadderRowHeightPx);
	return Settings.ClampLadderIndex(ActiveTarget.NumericType, StartLadderIndex + LadderIndexOffset);
}

#undef LOCTEXT_NAMESPACE
