#include "Input/ValueLadderInputProcessor.h"

#include "Input/ComponentTransformDetailsBridge.h"
#include "Input/ValueLadderTargetRegistry.h"
#include "UI/SValueLadderOverlay.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ValueLadderInputProcessor"

namespace
{
	constexpr float LadderRowHeightPx = ValueLadder::UI::LadderCellHeightPx;
	constexpr float OverlayWindowHeightPx = ValueLadder::UI::LadderViewportHeightPx + 8.0f;

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

	uint32 ToSlatePointerIndex(const int32 PointerIndex)
	{
		return static_cast<uint32>(FMath::Max(PointerIndex, 0));
	}
}

FValueLadderInputProcessor::FValueLadderInputProcessor()
{
}

void FValueLadderInputProcessor::CancelActiveGesture()
{
	if (bDragging)
	{
		EndGesture(false, TEXT("CancelActiveGesture"));
	}
}

void FValueLadderInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (bDragging && !SlateApp.IsActive())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] Slate application deactivated during gesture=%llu; cancelling and restoring cursor state."), ActiveGestureId);
		EndGesture(false, TEXT("SlateAppDeactivated"));
	}
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
	const bool bResolvedFromRegistry = FValueLadderTargetRegistry::Get().ResolveTargetFromWidgetPath(WidgetsUnderCursor, Target);
	const bool bResolvedFromTransformBridge = !bResolvedFromRegistry && FComponentTransformDetailsBridge::Get().ResolveTargetFromWidgetPath(WidgetsUnderCursor, Target);
	if (!bResolvedFromRegistry && !bResolvedFromTransformBridge)
	{
		UE_LOG(
			LogValueLadder,
			Verbose,
			TEXT("[Input] MouseDown rejected: no registered ValueLadder target resolved. Cursor=(%.1f, %.1f) WidgetPathDepth=%d"),
			CursorPosition.X,
			CursorPosition.Y,
			WidgetsUnderCursor.Widgets.Num());
		return false;
	}

	if (bResolvedFromTransformBridge)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Input] MouseDown resolved target through transform bridge fallback."));
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

	return InitializeGesture(*Settings, SlateApp, MouseEvent, WidgetsUnderCursor, Target);
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
		EndGesture(false, TEXT("MouseUpNullSettings"));
		return false;
	}

	if (MouseEvent.GetEffectingButton() != Settings->TriggerMouseButton)
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseUp ignored: button=%s expected=%s activeGesture=%llu"), *MouseEvent.GetEffectingButton().ToString(), *Settings->TriggerMouseButton.ToString(), ActiveGestureId);
		return true;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseUp finishing gesture=%llu commit=%s delta=%.6g"), ActiveGestureId, !FMath::IsNearlyZero(Session.GetCurrentDelta()) ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta());
	EndGesture(!FMath::IsNearlyZero(Session.GetCurrentDelta()), TEXT("TriggerMouseUp"));
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
		EndGesture(false, TEXT("MouseMoveNullSettings"));
		return false;
	}

	if (!MouseEvent.IsMouseButtonDown(Settings->TriggerMouseButton))
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] MouseMove observed released trigger button; ending gesture=%llu commit=%s delta=%.6g"), ActiveGestureId, !FMath::IsNearlyZero(Session.GetCurrentDelta()) ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta());
		EndGesture(!FMath::IsNearlyZero(Session.GetCurrentDelta()), TEXT("TriggerButtonReleasedDuringMove"));
		return true;
	}

	const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(ActiveUserIndex);
	const bool bHasCapture = SlateUser.IsValid() && SlateUser->HasCapture(ToSlatePointerIndex(ActivePointerIndex));
	const bool bOwnsCapture = HasOwnedCapture(SlateApp);
	const TSharedPtr<SWidget> CurrentCaptor = SlateUser.IsValid() ? SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)) : nullptr;
	const bool bHighPrecision = SlateApp.GetPlatformApplication().IsValid() && SlateApp.GetPlatformApplication()->IsUsingHighPrecisionMouseMode();
	bCursorLocked = bOwnsCapture && bHighPrecision;
	if (!bOwnsCapture)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove cancelling gesture=%llu because capture ownership was lost. ownsCapture=%s hasCapture=%s highPrecision=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasCapture ? TEXT("true") : TEXT("false"), bHighPrecision ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
		EndGesture(false, TEXT("CaptureOwnershipLost"));
		return true;
	}

	if (!bHighPrecision)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Input] MouseMove gesture=%llu is continuing without high precision mode. user=%d pointer=%d currentCaptor=%p"), ActiveGestureId, ActiveUserIndex, ActivePointerIndex, CurrentCaptor.Get());
	}

	const FVector2D CursorDelta = MouseEvent.GetCursorDelta();
	AccumulatedDragDelta += CursorDelta;

	const int32 ResolvedLadderIndex = ResolveActiveLadderIndex(*Settings);
	if (ResolvedLadderIndex != ActiveLadderIndex)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu switched ladder row via vertical drag: %d -> %d (verticalOffset=%.6g)"), ActiveGestureId, ActiveLadderIndex, ResolvedLadderIndex, AccumulatedDragDelta.Y);
		ActiveLadderIndex = ResolvedLadderIndex;
	}

	const double LadderStep = Settings->GetLadderStep(ActiveTarget.NumericType, ActiveLadderIndex);
	UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseMove gesture=%llu accumulatedX=%.6g accumulatedY=%.6g ladderIndex=%d ladderStep=%.6g ownsCapture=%s highPrecision=%s shift=%s ctrl=%s user=%d pointer=%d currentCaptor=%p"), ActiveGestureId, AccumulatedDragDelta.X, AccumulatedDragDelta.Y, ActiveLadderIndex, LadderStep, bOwnsCapture ? TEXT("true") : TEXT("false"), bHighPrecision ? TEXT("true") : TEXT("false"), MouseEvent.IsShiftDown() ? TEXT("true") : TEXT("false"), MouseEvent.IsControlDown() ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CurrentCaptor.Get());
	FString SessionError;
	if (!Session.UpdateFromPixelOffset(AccumulatedDragDelta.X, LadderStep, MouseEvent.IsShiftDown(), MouseEvent.IsControlDown(), *Settings, SessionError))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove update failed for gesture=%llu: %s"), ActiveGestureId, *SessionError);
		EndGesture(false, TEXT("SessionUpdateFailed"));
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
		EndGesture(false, TEXT("EscapePressed"));
		return true;
	}

	return false;
}

bool FValueLadderInputProcessor::InitializeGesture(const UValueLadderSettings& Settings, FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, const FWidgetPath& WidgetsUnderCursor, const FValueLadderPropertyTarget& Target)
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
	bCursorLocked = false;
	ActiveTarget = Target;
	GestureEventPath = WidgetsUnderCursor;
	CaptureWidget = GestureEventPath.IsValid() ? TWeakPtr<SWidget>(GestureEventPath.GetLastWidget()) : nullptr;
	ActiveUserIndex = MouseEvent.GetUserIndex();
	ActivePointerIndex = static_cast<int32>(ETouchIndex::CursorPointerIndex);
	DragStartPosition = MouseEvent.GetScreenSpacePosition();
	CursorRestorePosition = DragStartPosition;
	AccumulatedDragDelta = FVector2D::ZeroVector;
	Settings.BuildLadderDisplayValues(Target.NumericType, ActiveLadderValues);
	StartLadderIndex = Settings.GetDefaultLadderIndex(Target.NumericType);
	ActiveLadderIndex = StartLadderIndex;
	OverlayAnchorPosition = DragStartPosition - FVector2D(44.0f, 8.0f + static_cast<float>(StartLadderIndex) * LadderRowHeightPx);
	UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu initialized. Type=%s StartIndex=%d LadderCount=%d Anchor=(%.1f, %.1f) CursorRestore=(%.1f, %.1f) user=%d pointer=%d captureWidget=%p"), ActiveGestureId, ToNumericTypeString(Target.NumericType), StartLadderIndex, ActiveLadderValues.Num(), OverlayAnchorPosition.X, OverlayAnchorPosition.Y, CursorRestorePosition.X, CursorRestorePosition.Y, ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get());
	if (ActiveLadderValues.Num() == 0)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu initialized with zero ladder rows. type=%s startIndex=%d"), ActiveGestureId, ToNumericTypeString(Target.NumericType), StartLadderIndex);
	}

	if (Settings.bShowOverlay)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu requested overlay creation."), ActiveGestureId);
		EnsureOverlay(SlateApp);
	}
	else
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu did not create overlay because bShowOverlay=false."), ActiveGestureId);
	}

	if (!BeginSlateCapture(SlateApp))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] Gesture=%llu failed to enter Slate capture/high-precision mode."), ActiveGestureId);
		EndGesture(false, TEXT("BeginSlateCaptureFailed"));
		return false;
	}

	if (Settings.bShowOverlay)
	{
		UpdateOverlay();
	}

	return true;
}

void FValueLadderInputProcessor::EnsureOverlay(FSlateApplication& SlateApp)
{
	const bool bWindowValid = OverlayWindow.IsValid();
	const bool bWidgetValid = OverlayWidget.IsValid();
	if (bWindowValid && bWidgetValid)
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Overlay] Gesture=%llu reused existing overlay window."), ActiveGestureId);
		return;
	}

	if (bWindowValid || bWidgetValid)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu detected stale overlay state before creation. windowValid=%s widgetValid=%s; recreating."), ActiveGestureId, bWindowValid ? TEXT("true") : TEXT("false"), bWidgetValid ? TEXT("true") : TEXT("false"));
		DestroyOverlay();
	}

	const FVector2D RawAnchorPosition = OverlayAnchorPosition;
	const FSlateRect PreferredWorkArea = SlateApp.GetPreferredWorkArea();
	OverlayAnchorPosition.X = FMath::Clamp(OverlayAnchorPosition.X, PreferredWorkArea.Left, FMath::Max(PreferredWorkArea.Left, PreferredWorkArea.Right - ValueLadder::UI::OverlayWidthPx));
	OverlayAnchorPosition.Y = FMath::Clamp(OverlayAnchorPosition.Y, PreferredWorkArea.Top, FMath::Max(PreferredWorkArea.Top, PreferredWorkArea.Bottom - OverlayWindowHeightPx));
	UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu anchor raw=(%.1f, %.1f) clamped=(%.1f, %.1f) workArea=(%.1f, %.1f, %.1f, %.1f)"), ActiveGestureId, RawAnchorPosition.X, RawAnchorPosition.Y, OverlayAnchorPosition.X, OverlayAnchorPosition.Y, PreferredWorkArea.Left, PreferredWorkArea.Top, PreferredWorkArea.Right, PreferredWorkArea.Bottom);

	const uint64 RequestedGestureId = ActiveGestureId;
	TSharedRef<SValueLadderOverlay> NewOverlayWidget = SNew(SValueLadderOverlay);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		.ScreenPosition(OverlayAnchorPosition)
		[
			NewOverlayWidget
		];

	SlateApp.AddWindow(Window, true);
	if (!bDragging || ActiveGestureId != RequestedGestureId)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture changed while creating overlay window. requestedGesture=%llu activeGesture=%llu dragging=%s; destroying transient window=%p"), RequestedGestureId, ActiveGestureId, bDragging ? TEXT("true") : TEXT("false"), &Window.Get());
		Window->RequestDestroyWindow();
		return;
	}

	OverlayWidget = NewOverlayWidget;
	OverlayWindow = Window;
	UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu created overlay window=%p widget=%p at (%.1f, %.1f) rows=%d activeIndex=%d"), ActiveGestureId, &Window.Get(), OverlayWidget.Get(), OverlayAnchorPosition.X, OverlayAnchorPosition.Y, ActiveLadderValues.Num(), ActiveLadderIndex);
}

void FValueLadderInputProcessor::UpdateOverlay()
{
	if (OverlayWidget.IsValid())
	{
		if (ActiveLadderValues.Num() == 0)
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu update has zero ladder rows. windowValid=%s activeIndex=%d previewLen=%d"), ActiveGestureId, OverlayWindow.IsValid() ? TEXT("true") : TEXT("false"), ActiveLadderIndex, Session.GetPreviewValueText().Len());
		}
		else if (!ActiveLadderValues.IsValidIndex(ActiveLadderIndex))
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu update has invalid active index=%d for rows=%d. windowValid=%s"), ActiveGestureId, ActiveLadderIndex, ActiveLadderValues.Num(), OverlayWindow.IsValid() ? TEXT("true") : TEXT("false"));
		}

		const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();
		const double TickThresholdPx = Settings != nullptr ? static_cast<double>(Settings->DragActivationThresholdPx) : 12.0;
		const double TickValueDelta = Settings != nullptr
			? Settings->GetLadderStep(ActiveTarget.NumericType, ActiveLadderIndex) * Session.GetCurrentMultiplier()
			: 0.0;
		const double PixelsToNextTick = Session.GetCurrentPixelsToNextTick() > 0.0 ? Session.GetCurrentPixelsToNextTick() : TickThresholdPx;
		OverlayWidget->UpdateDisplay(ActiveLadderValues, ActiveLadderIndex, Session.GetCurrentMultiplier(), Session.GetCurrentDelta(), Session.GetPreviewValueText(), bCursorLocked, Session.GetCurrentTickCount(), Session.GetCurrentTickProgress(), PixelsToNextTick, TickThresholdPx, TickValueDelta, AccumulatedDragDelta.Y, AccumulatedDragDelta.X);
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Overlay] Gesture=%llu updated overlay. ActiveIndex=%d CursorLocked=%s Delta=%.6g Multiplier=%.6g"), ActiveGestureId, ActiveLadderIndex, bCursorLocked ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta(), Session.GetCurrentMultiplier());
	}
	else
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu update requested but overlay widget is invalid. windowValid=%s rows=%d activeIndex=%d"), ActiveGestureId, OverlayWindow.IsValid() ? TEXT("true") : TEXT("false"), ActiveLadderValues.Num(), ActiveLadderIndex);
	}
}

void FValueLadderInputProcessor::DestroyOverlay()
{
	if (OverlayWindow.IsValid())
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu destroying overlay window=%p widget=%p rows=%d activeIndex=%d"), ActiveGestureId, OverlayWindow.Pin().Get(), OverlayWidget.Get(), ActiveLadderValues.Num(), ActiveLadderIndex);
		OverlayWindow.Pin()->RequestDestroyWindow();
		OverlayWindow.Reset();
	}
	else
	{
		UE_LOG(LogValueLadder, Verbose, TEXT("[Overlay] Gesture=%llu destroy requested but overlay window was already invalid."), ActiveGestureId);
	}

	OverlayWidget.Reset();
}

void FValueLadderInputProcessor::EndGesture(const bool bCommit, const TCHAR* Reason)
{
	UE_LOG(LogValueLadder, Display, TEXT("[Input] EndGesture gesture=%llu reason=%s commit=%s delta=%.6g overlayWidget=%s overlayWindow=%s rows=%d activeIndex=%d"), ActiveGestureId, Reason != nullptr ? Reason : TEXT("<null>"), bCommit ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta(), OverlayWidget.IsValid() ? TEXT("true") : TEXT("false"), OverlayWindow.IsValid() ? TEXT("true") : TEXT("false"), ActiveLadderValues.Num(), ActiveLadderIndex);
	FSlateApplication& SlateApp = FSlateApplication::Get();
	EndSlateCapture(SlateApp);
	DestroyOverlay();

	if (bCommit)
	{
		Session.Commit();
	}
	else
	{
		Session.Cancel();
	}

	ResetGestureState();
}

void FValueLadderInputProcessor::ResetGestureState()
{
	bDragging = false;
	bCursorLocked = false;
	bPendingCursorRestore = false;
	DragStartPosition = FVector2D::ZeroVector;
	CursorRestorePosition = FVector2D::ZeroVector;
	ActiveTarget = FValueLadderPropertyTarget();
	ActiveLadderValues.Reset();
	StartLadderIndex = 0;
	ActiveLadderIndex = 0;
	AccumulatedDragDelta = FVector2D::ZeroVector;
	OverlayAnchorPosition = FVector2D::ZeroVector;
	GestureEventPath = FWidgetPath();
	CaptureWidget.Reset();
	ActivePointerIndex = 10;
	ActiveUserIndex = 0;
	ActiveGestureId = 0;
}

int32 FValueLadderInputProcessor::ResolveActiveLadderIndex(const UValueLadderSettings& Settings) const
{
	const int32 ResolvedIndex = ValueLadder::Math::ResolveLadderIndexFromVerticalOffset(StartLadderIndex, AccumulatedDragDelta.Y, LadderRowHeightPx, 0, ActiveLadderValues.Num() - 1);
	return Settings.ClampLadderIndex(ActiveTarget.NumericType, ResolvedIndex);
}

bool FValueLadderInputProcessor::BeginSlateCapture(FSlateApplication& SlateApp)
{
	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	if (!GestureEventPath.IsValid() || GestureEventPath.Widgets.Num() == 0 || !PinnedCaptureWidget.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] BeginSlateCapture failed for gesture=%llu because the event path or capture widget was invalid. pathValid=%s captureWidget=%p"), ActiveGestureId, GestureEventPath.IsValid() ? TEXT("true") : TEXT("false"), PinnedCaptureWidget.Get());
		return false;
	}

	const FWidgetPath CapturePath = BuildCapturePath();
	if (!CapturePath.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] BeginSlateCapture failed for gesture=%llu because capture path construction returned an invalid path. captureWidget=%p"), ActiveGestureId, PinnedCaptureWidget.Get());
		return false;
	}

	const FReply Reply = FReply::Handled()
		.CaptureMouse(PinnedCaptureWidget.ToSharedRef())
		.UseHighPrecisionMouseMovement(PinnedCaptureWidget.ToSharedRef())
		.SetUserFocus(PinnedCaptureWidget.ToSharedRef(), EFocusCause::Mouse)
		.LockMouseToWidget(PinnedCaptureWidget.ToSharedRef())
		.SetMousePos(FIntPoint(FMath::RoundToInt(CursorRestorePosition.X), FMath::RoundToInt(CursorRestorePosition.Y)))
		.PreventThrottling();
	SlateApp.ProcessExternalReply(CapturePath, Reply, ActiveUserIndex, ActivePointerIndex);

	const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(ActiveUserIndex);
	const bool bHasCapture = SlateUser.IsValid() && SlateUser->HasCapture(ToSlatePointerIndex(ActivePointerIndex));
	const bool bOwnsCapture = HasOwnedCapture(SlateApp);
	const TSharedPtr<SWidget> CurrentCaptor = SlateUser.IsValid() ? SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)) : nullptr;
	const bool bHighPrecision = SlateApp.GetPlatformApplication().IsValid() && SlateApp.GetPlatformApplication()->IsUsingHighPrecisionMouseMode();
	bCursorLocked = bOwnsCapture && bHighPrecision;
	UE_LOG(LogValueLadder, Display, TEXT("[Input] BeginSlateCapture gesture=%llu ownsCapture=%s hasCapture=%s highPrecision=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p pathLength=%d"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasCapture ? TEXT("true") : TEXT("false"), bHighPrecision ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, PinnedCaptureWidget.Get(), CurrentCaptor.Get(), CapturePath.Widgets.Num());
	return bOwnsCapture;
}

void FValueLadderInputProcessor::EndSlateCapture(FSlateApplication& SlateApp)
{
	if (!bDragging)
	{
		return;
	}

	const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(ActiveUserIndex);
	const TSharedPtr<SWidget> CurrentCaptor = SlateUser.IsValid() ? SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)) : nullptr;
	const bool bOwnsCapture = HasOwnedCapture(SlateApp);
	FWidgetPath CapturePath = BuildCapturePath();
	if (bOwnsCapture && !CapturePath.IsValid())
	{
		const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin().IsValid() ? CaptureWidget.Pin() : CurrentCaptor;
		if (PinnedCaptureWidget.IsValid())
		{
			SlateApp.GeneratePathToWidgetUnchecked(PinnedCaptureWidget.ToSharedRef(), CapturePath, EVisibility::Visible);
		}
	}
	const bool bHasValidPath = CapturePath.IsValid();

	if (bOwnsCapture && bHasValidPath)
	{
		const FReply Reply = FReply::Handled()
			.ReleaseMouseCapture()
			.ReleaseMouseLock()
			.SetMousePos(FIntPoint(FMath::RoundToInt(CursorRestorePosition.X), FMath::RoundToInt(CursorRestorePosition.Y)));
		SlateApp.ProcessExternalReply(CapturePath, Reply, ActiveUserIndex, ActivePointerIndex);
		UE_LOG(LogValueLadder, Display, TEXT("[Input] EndSlateCapture released gesture=%llu ownsCapture=%s pathValid=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasValidPath ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
	}
	else
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] EndSlateCapture skipped release for gesture=%llu because ownership/path validation failed. ownsCapture=%s pathValid=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasValidPath ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
	}

	bCursorLocked = false;
}

bool FValueLadderInputProcessor::HasOwnedCapture(FSlateApplication& SlateApp) const
{
	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(ActiveUserIndex);
	if (!PinnedCaptureWidget.IsValid() || !SlateUser.IsValid())
	{
		return false;
	}

	return SlateUser->DoesWidgetHaveCapture(PinnedCaptureWidget, ToSlatePointerIndex(ActivePointerIndex));
}

FWidgetPath FValueLadderInputProcessor::BuildCapturePath() const
{
	if (!GestureEventPath.IsValid())
	{
		return FWidgetPath();
	}

	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	if (!PinnedCaptureWidget.IsValid() || !GestureEventPath.ContainsWidget(PinnedCaptureWidget.Get()))
	{
		return FWidgetPath();
	}

	return GestureEventPath.GetPathDownTo(PinnedCaptureWidget.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
