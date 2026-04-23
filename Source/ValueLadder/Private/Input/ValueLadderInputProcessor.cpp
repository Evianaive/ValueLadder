#include "Input/ValueLadderInputProcessor.h"

#include "Input/ComponentTransformDetailsBridge.h"
#include "Input/ValueLadderTargetRegistry.h"
#include "Input/ValueLadderUnitMetadata.h"
#include "UI/SValueLadderOverlay.h"
#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InputCoreTypes.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ValueLadderInputProcessor"

namespace
{
	constexpr float LadderRowHeightPx = ValueLadder::UI::LadderRowStridePx;
	constexpr float OverlayWindowHeightPx = ValueLadder::UI::LadderViewportHeightPx + 8.0f;
	const FName VLT_NAME_ForceUnits(TEXT("ForceUnits"));
	const FName VLT_NAME_Units(TEXT("Units"));
	const FName VLT_NAME_Translation(TEXT("Translation"));
	const FName VLT_NAME_Rotation(TEXT("Rotation"));
	const FName VLT_NAME_Scale3D(TEXT("Scale3D"));

	const TCHAR* ToNumericTypeString(const EValueLadderNumericType NumericType)
	{
		return ValueLadder::ToNumericTypeString(NumericType);
	}

	bool IsNumericBarWidgetType(const FString& WidgetType)
	{
		return WidgetType.Contains(TEXT("SNumericEntryBox"))
			|| WidgetType.Contains(TEXT("SSpinBox"))
			|| WidgetType.Contains(TEXT("SNumericVectorInputBox"))
			|| WidgetType.Contains(TEXT("SNumericRotatorInputBox"));
	}

	bool IsPreferredCaptureWidgetType(const FString& WidgetType)
	{
		return WidgetType.Contains(TEXT("SNumericVectorInputBox"))
			|| WidgetType.Contains(TEXT("SNumericRotatorInputBox"))
			|| WidgetType.Contains(TEXT("SNumericEntryBox"))
			|| WidgetType.Contains(TEXT("SSpinBox"));
	}

	int32 GetCaptureWidgetPriority(const FString& WidgetType)
	{
		if (WidgetType.Contains(TEXT("SNumericVectorInputBox")) || WidgetType.Contains(TEXT("SNumericRotatorInputBox")))
		{
			return 3;
		}

		if (WidgetType.Contains(TEXT("SNumericEntryBox")))
		{
			return 2;
		}

		if (WidgetType.Contains(TEXT("SSpinBox")))
		{
			return 1;
		}

		return 0;
	}



		bool IsNumericBarWidgetPath(const FWidgetPath& WidgetPath)
	{
		for (int32 WidgetIndex = 0; WidgetIndex < WidgetPath.Widgets.Num(); ++WidgetIndex)
		{
			const FArrangedWidget& ArrangedWidget = WidgetPath.Widgets[WidgetIndex];
			if (IsNumericBarWidgetType(ArrangedWidget.Widget->GetTypeAsString()))
			{
				return true;
			}
		}

		return false;
	}

	uint32 ToSlatePointerIndex(const int32 PointerIndex)
	{
		return static_cast<uint32>(FMath::Max(PointerIndex, 0));
	}

	TSharedPtr<IPropertyHandle> FindChildHandleByPropertyName(const TSharedRef<IPropertyHandle>& PropertyHandle, const FName& PropertyName)
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

	void AppendHandleChain(TSharedPtr<IPropertyHandle> Handle, TArray<TSharedPtr<IPropertyHandle>>& OutHandles)
	{
		while (Handle.IsValid() && Handle->IsValidHandle())
		{
			OutHandles.Add(Handle);
			Handle = Handle->GetParentHandle();
		}
	}

	TArray<FValueLadderUnitMetadata> BuildUnitMetadataChain(const TArray<TSharedPtr<IPropertyHandle>>& CandidateHandles)
	{
		TArray<FValueLadderUnitMetadata> MetadataChain;
		MetadataChain.Reserve(CandidateHandles.Num());
		for (const TSharedPtr<IPropertyHandle>& CandidateHandle : CandidateHandles)
		{
			if (!CandidateHandle.IsValid() || !CandidateHandle->IsValidHandle())
			{
				continue;
			}

			FValueLadderUnitMetadata Metadata;
			Metadata.ForceUnits = CandidateHandle->GetMetaData(VLT_NAME_ForceUnits);
			Metadata.Units = CandidateHandle->GetMetaData(VLT_NAME_Units);
			MetadataChain.Add(Metadata);
		}

		return MetadataChain;
	}

	FName GetTransformGroupName(const FValueLadderPropertyTarget::ETransformField TransformField)
	{
		switch (TransformField)
		{
		case FValueLadderPropertyTarget::ETransformField::Location:
			return VLT_NAME_Translation;
		case FValueLadderPropertyTarget::ETransformField::Rotation:
			return VLT_NAME_Rotation;
		case FValueLadderPropertyTarget::ETransformField::Scale:
			return VLT_NAME_Scale3D;
		default:
			return NAME_None;
		}
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
		MaybeShowUnsupportedTargetNotification(WidgetsUnderCursor, LOCTEXT("UnsupportedNumericControl", "Value Ladder cannot edit this numeric control yet."));
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
		MaybeShowUnsupportedTargetNotification(WidgetsUnderCursor, LOCTEXT("DisabledNumericType", "Value Ladder is disabled for this numeric type."));
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
	const TSharedPtr<SWidget> CurrentCaptor = SlateUser.IsValid() ? SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)) : nullptr;
	const bool bOwnsCapture = HasOwnedCapture(SlateApp);
	bCursorLocked = false;
	if (!bOwnsCapture)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove cancelling gesture=%llu because capture ownership was lost. ownsCapture=%s hasCapture=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasCapture ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
		EndGesture(false, TEXT("CaptureOwnershipLost"));
		return true;
	}

	const FVector2D PointerPosition = MouseEvent.GetScreenSpacePosition();
	const bool bPointerInSelectionLane = Settings->bShowOverlay && IsPointerInSelectionLane(PointerPosition);
	if (bPointerInSelectionLane)
	{
		if (bRowLocked)
		{
			FString ResetError;
			if (!Session.ResetDeltaContext(ResetError))
			{
				UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove failed to reset delta context while re-entering selection lane for gesture=%llu: %s"), ActiveGestureId, *ResetError);
				EndGesture(false, TEXT("SelectionLaneResetFailed"));
				return true;
			}

			UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu re-entered selection lane; unlocking row selection at (%.1f, %.1f)"), ActiveGestureId, PointerPosition.X, PointerPosition.Y);
			LockReferencePosition = PointerPosition;
			AccumulatedDragDelta = FVector2D::ZeroVector;
		}

		bRowLocked = false;
		bHasEnteredSelectionLane = true;

		const int32 ResolvedLadderIndex = ResolveHoveredLadderIndex(*Settings, PointerPosition);
		if (ResolvedLadderIndex != ActiveLadderIndex)
		{
			UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu hovered ladder row changed: %d -> %d"), ActiveGestureId, ActiveLadderIndex, ResolvedLadderIndex);
			ActiveLadderIndex = ResolvedLadderIndex;
		}

		UpdateOverlay();
		return true;
	}

	if (!bHasEnteredSelectionLane)
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] Gesture=%llu waiting for pointer to enter selection lane before locking. pointer=(%.1f, %.1f)"), ActiveGestureId, PointerPosition.X, PointerPosition.Y);
		UpdateOverlay();
		return true;
	}

	if (!bRowLocked)
	{
		FString ResetError;
		if (!Session.ResetDeltaContext(ResetError))
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Input] MouseMove failed to reset delta context while locking row for gesture=%llu: %s"), ActiveGestureId, *ResetError);
			EndGesture(false, TEXT("RowLockResetFailed"));
			return true;
		}

		bRowLocked = true;
		LockReferencePosition = PointerPosition;
		AccumulatedDragDelta = FVector2D::ZeroVector;
		UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu locked ladder row=%d after leaving selection lane at (%.1f, %.1f)"), ActiveGestureId, ActiveLadderIndex, PointerPosition.X, PointerPosition.Y);
	}

	AccumulatedDragDelta.X = PointerPosition.X - LockReferencePosition.X;
	AccumulatedDragDelta.Y = PointerPosition.Y - DragStartPosition.Y;

	const EValueLadderSemanticRole SemanticRole = ActiveTarget.SemanticRole;
	const double LadderStep = Settings->GetLadderStep(ActiveTarget.NumericType, ActiveLadderIndex, ActiveUnitKey, SemanticRole);
	UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Input] MouseMove gesture=%llu rowLocked=%s accumulatedX=%.6g accumulatedY=%.6g ladderIndex=%d ladderStep=%.6g ownsCapture=%s shift=%s ctrl=%s user=%d pointer=%d currentCaptor=%p"), ActiveGestureId, bRowLocked ? TEXT("true") : TEXT("false"), AccumulatedDragDelta.X, AccumulatedDragDelta.Y, ActiveLadderIndex, LadderStep, bOwnsCapture ? TEXT("true") : TEXT("false"), MouseEvent.IsShiftDown() ? TEXT("true") : TEXT("false"), MouseEvent.IsControlDown() ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CurrentCaptor.Get());
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

void FValueLadderInputProcessor::MaybeShowUnsupportedTargetNotification(const FWidgetPath& WidgetsUnderCursor, const FText& Message)
{
	if (!IsNumericBarWidgetPath(WidgetsUnderCursor))
	{
		return;
	}

	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	if (CurrentTimeSeconds - LastUnsupportedNotificationTimeSeconds < 1.0)
	{
		return;
	}

	LastUnsupportedNotificationTimeSeconds = CurrentTimeSeconds;

	FNotificationInfo NotificationInfo(Message);
	NotificationInfo.ExpireDuration = 2.5f;
	NotificationInfo.FadeOutDuration = 0.2f;
	NotificationInfo.bFireAndForget = true;
	NotificationInfo.bUseLargeFont = false;
	NotificationInfo.bUseSuccessFailIcons = false;
	FSlateNotificationManager::Get().AddNotification(NotificationInfo);
}

TSharedPtr<SWidget> FValueLadderInputProcessor::ResolveCaptureWidget(const FWidgetPath& WidgetPath) const
{
	TSharedPtr<SWidget> BestWidget;
	int32 BestPriority = 0;

	for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
	{
		const TSharedRef<SWidget>& CandidateWidget = WidgetPath.Widgets[WidgetIndex].Widget;
		const FString WidgetType = CandidateWidget->GetTypeAsString();
		if (!IsPreferredCaptureWidgetType(WidgetType))
		{
			continue;
		}

		const int32 Priority = GetCaptureWidgetPriority(WidgetType);
		if (Priority > BestPriority)
		{
			BestPriority = Priority;
			BestWidget = CandidateWidget;
			if (Priority >= 3)
			{
				break;
			}
		}
	}

	return BestWidget;
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
	CaptureWidget = ResolveCaptureWidget(GestureEventPath);
	if (!CaptureWidget.IsValid() && GestureEventPath.IsValid())
	{
		CaptureWidget = TWeakPtr<SWidget>(GestureEventPath.GetLastWidget());
	}
	ActiveUserIndex = MouseEvent.GetUserIndex();
	ActivePointerIndex = static_cast<int32>(ETouchIndex::CursorPointerIndex);
	DragStartPosition = MouseEvent.GetScreenSpacePosition();
	CursorRestorePosition = DragStartPosition;
	LockReferencePosition = DragStartPosition;
	AccumulatedDragDelta = FVector2D::ZeroVector;
	bRowLocked = false;
	bHasEnteredSelectionLane = !Settings.bShowOverlay;
	ActiveUnitKey = ResolveTargetUnitKey(Target);
	Settings.BuildLadderDisplayValues(Target.NumericType, ActiveLadderValues, ActiveUnitKey, Target.SemanticRole);
	StartLadderIndex = Settings.GetDefaultLadderIndex(Target.NumericType, ActiveUnitKey, Target.SemanticRole);
	ActiveLadderIndex = StartLadderIndex;
	OverlayAnchorPosition = DragStartPosition - FVector2D(44.0f, 8.0f + static_cast<float>(StartLadderIndex) * LadderRowHeightPx);
	UE_LOG(LogValueLadder, Display, TEXT("[Input] Gesture=%llu initialized. Type=%s Unit=%s StartIndex=%d LadderCount=%d Anchor=(%.1f, %.1f) CursorRestore=(%.1f, %.1f) user=%d pointer=%d captureWidget=%p"), ActiveGestureId, ToNumericTypeString(Target.NumericType), ActiveUnitKey.IsNone() ? TEXT("<none>") : *ActiveUnitKey.ToString(), StartLadderIndex, ActiveLadderValues.Num(), OverlayAnchorPosition.X, OverlayAnchorPosition.Y, CursorRestorePosition.X, CursorRestorePosition.Y, ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get());
	if (ActiveLadderValues.Num() == 0)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Overlay] Gesture=%llu initialized with zero ladder rows. type=%s startIndex=%d"), ActiveGestureId, ToNumericTypeString(Target.NumericType), StartLadderIndex);
	}

	if (Settings.bShowOverlay)
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu requested overlay creation."), ActiveGestureId);
		EnsureOverlay(SlateApp);
		bHasEnteredSelectionLane = IsPointerInSelectionLane(DragStartPosition);
	}
	else
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Overlay] Gesture=%llu did not create overlay because bShowOverlay=false."), ActiveGestureId);
	}

	if (!BeginSlateCapture(SlateApp))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] Gesture=%llu failed to enter Slate capture mode."), ActiveGestureId);
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
		.IsTopmostWindow(true)
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
			? Settings->GetLadderStep(ActiveTarget.NumericType, ActiveLadderIndex, ActiveUnitKey, ActiveTarget.SemanticRole) * Session.GetCurrentMultiplier()
			: 0.0;
		const double PixelsToNextTick = Session.GetCurrentPixelsToNextTick() > 0.0 ? Session.GetCurrentPixelsToNextTick() : TickThresholdPx;
		OverlayWidget->UpdateDisplay(ActiveLadderValues, ActiveLadderIndex, Session.GetCurrentMultiplier(), Session.GetCurrentDelta(), Session.GetPreviewValueText(), bRowLocked, Session.GetCurrentTickCount(), Session.GetCurrentTickProgress(), PixelsToNextTick, TickThresholdPx, TickValueDelta, 0.0, AccumulatedDragDelta.X);
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Overlay] Gesture=%llu updated overlay. ActiveIndex=%d RowLocked=%s Delta=%.6g Multiplier=%.6g"), ActiveGestureId, ActiveLadderIndex, bRowLocked ? TEXT("true") : TEXT("false"), Session.GetCurrentDelta(), Session.GetCurrentMultiplier());
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
	bRowLocked = false;
	bHasEnteredSelectionLane = false;
	bPendingCursorRestore = false;
	DragStartPosition = FVector2D::ZeroVector;
	CursorRestorePosition = FVector2D::ZeroVector;
	LockReferencePosition = FVector2D::ZeroVector;
	ActiveTarget = FValueLadderPropertyTarget();
	ActiveLadderValues.Reset();
	ActiveUnitKey = NAME_None;
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

FName FValueLadderInputProcessor::ResolveTargetUnitKey(const FValueLadderPropertyTarget& Target) const
{
	if (!Target.PropertyHandle.IsValid() || !Target.PropertyHandle->IsValidHandle())
	{
		return NAME_None;
	}

	TArray<TSharedPtr<IPropertyHandle>> CandidateHandles;
	if (Target.Kind == FValueLadderPropertyTarget::ETargetKind::TransformProxy)
	{
		const FName GroupName = GetTransformGroupName(Target.TransformField);
		const TSharedPtr<IPropertyHandle> GroupHandle = GroupName.IsNone() ? nullptr : FindChildHandleByPropertyName(Target.PropertyHandle.ToSharedRef(), GroupName);
		const TSharedPtr<IPropertyHandle> ComponentHandle = GroupHandle.IsValid() && !Target.ComponentName.IsNone()
			? FindChildHandleByPropertyName(GroupHandle.ToSharedRef(), Target.ComponentName)
			: nullptr;

		if (ComponentHandle.IsValid())
		{
			AppendHandleChain(ComponentHandle, CandidateHandles);
		}
		else if (GroupHandle.IsValid())
		{
			AppendHandleChain(GroupHandle, CandidateHandles);
		}
		else
		{
			AppendHandleChain(Target.PropertyHandle, CandidateHandles);
		}
	}
	else
	{
		AppendHandleChain(Target.PropertyHandle, CandidateHandles);
	}

	return ValueLadder::Units::ResolveUnitKey(BuildUnitMetadataChain(CandidateHandles));
}

bool FValueLadderInputProcessor::IsPointerInSelectionLane(const FVector2D& PointerScreenPosition) const
{
	const float LaneLeft = OverlayAnchorPosition.X + ValueLadder::UI::OverlayChromeInsetPx;
	const float LaneRight = LaneLeft + ValueLadder::UI::LadderListWidthPx;
	const float LaneTop = OverlayAnchorPosition.Y + ValueLadder::UI::OverlayChromeInsetPx;
	const float ContentHeightPx = ActiveLadderValues.Num() > 0
		? static_cast<float>(ActiveLadderValues.Num()) * ValueLadder::UI::LadderRowStridePx - ValueLadder::UI::LadderRowSpacingPx
		: ValueLadder::UI::LadderCellHeightPx;
	const float LaneBottom = OverlayAnchorPosition.Y + ContentHeightPx;
	return PointerScreenPosition.X >= LaneLeft && PointerScreenPosition.X <= LaneRight && PointerScreenPosition.Y >= LaneTop && PointerScreenPosition.Y <= LaneBottom;
}

int32 FValueLadderInputProcessor::ResolveHoveredLadderIndex(const UValueLadderSettings& Settings, const FVector2D& PointerScreenPosition) const
{
	const float RelativeY = FMath::Max(0.0f, PointerScreenPosition.Y - (OverlayAnchorPosition.Y + ValueLadder::UI::OverlayChromeInsetPx));
	const int32 HoveredIndex = FMath::FloorToInt(RelativeY / LadderRowHeightPx);
	return Settings.ClampLadderIndex(ActiveTarget.NumericType, HoveredIndex, ActiveUnitKey, ActiveTarget.SemanticRole);
}

bool FValueLadderInputProcessor::BeginSlateCapture(FSlateApplication& SlateApp)
{
	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	if (!GestureEventPath.IsValid() || GestureEventPath.Widgets.Num() == 0 || !PinnedCaptureWidget.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Input] BeginSlateCapture failed for gesture=%llu because the event path or capture widget was invalid. pathValid=%s captureWidget=%p"), ActiveGestureId, GestureEventPath.IsValid() ? TEXT("true") : TEXT("false"), PinnedCaptureWidget.Get());
		return false;
	}

	auto TryCaptureWithWidget = [this, &SlateApp](const TSharedRef<SWidget>& CandidateWidget, const TCHAR* AttemptLabel) -> bool
	{
		CaptureWidget = CandidateWidget;
		const FWidgetPath CapturePath = BuildCapturePath();
		if (!CapturePath.IsValid())
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Input] BeginSlateCapture attempt=%s failed for gesture=%llu because capture path construction returned an invalid path. captureWidget=%p type=%s"), AttemptLabel, ActiveGestureId, &CandidateWidget.Get(), *CandidateWidget->GetTypeAsString());
			return false;
		}

		FReply Reply = FReply::Handled()
			.CaptureMouse(CandidateWidget)
			.PreventThrottling();

		const FString WidgetType = CandidateWidget->GetTypeAsString();
		const bool bShouldPreserveExistingFocus = FCString::Stricmp(AttemptLabel, TEXT("leaf-retry")) == 0
			&& (WidgetType.Contains(TEXT("EditableText")) || WidgetType.Contains(TEXT("SEditableTextBox")));
		if (!bShouldPreserveExistingFocus)
		{
			Reply = Reply.SetUserFocus(CandidateWidget, EFocusCause::Mouse);
		}
		SlateApp.ProcessExternalReply(CapturePath, Reply, ActiveUserIndex, ActivePointerIndex);

		const TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(ActiveUserIndex);
		const bool bHasCapture = SlateUser.IsValid() && SlateUser->HasCapture(ToSlatePointerIndex(ActivePointerIndex));
		const TSharedPtr<SWidget> CurrentCaptor = SlateUser.IsValid() ? SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)) : nullptr;
		const bool bOwnsCapture = HasOwnedCapture(SlateApp);
		UE_LOG(LogValueLadder, Display, TEXT("[Input] BeginSlateCapture gesture=%llu attempt=%s widgetType=%s ownsCapture=%s hasCapture=%s preserveFocus=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p pathLength=%d"), ActiveGestureId, AttemptLabel, *WidgetType, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasCapture ? TEXT("true") : TEXT("false"), bShouldPreserveExistingFocus ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, &CandidateWidget.Get(), CurrentCaptor.Get(), CapturePath.Widgets.Num());
		return bOwnsCapture;
	};

	bCursorLocked = false;
	if (TryCaptureWithWidget(PinnedCaptureWidget.ToSharedRef(), TEXT("initial")))
	{
		return true;
	}

	const TSharedRef<SWidget> FallbackWidget = GestureEventPath.GetLastWidget();
	if (&FallbackWidget.Get() != PinnedCaptureWidget.Get())
	{
		UE_LOG(LogValueLadder, Display, TEXT("[Input] BeginSlateCapture retrying leaf widget for gesture=%llu initialWidget=%p initialType=%s fallbackWidget=%p fallbackType=%s"), ActiveGestureId, PinnedCaptureWidget.Get(), *PinnedCaptureWidget->GetTypeAsString(), &FallbackWidget.Get(), *FallbackWidget->GetTypeAsString());
		if (TryCaptureWithWidget(FallbackWidget, TEXT("leaf-retry")))
		{
			return true;
		}
	}

	CaptureWidget = PinnedCaptureWidget;
	return false;
}

bool FValueLadderInputProcessor::DoesCaptorMatchExpectedWidget(const TSharedPtr<SWidget>& CurrentCaptor) const
{
	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	if (!PinnedCaptureWidget.IsValid() || !CurrentCaptor.IsValid())
	{
		return false;
	}

	if (CurrentCaptor.Get() == PinnedCaptureWidget.Get())
	{
		return true;
	}

	return GestureEventPath.IsValid()
		&& GestureEventPath.ContainsWidget(PinnedCaptureWidget.Get())
		&& GestureEventPath.ContainsWidget(CurrentCaptor.Get());
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
	if (bOwnsCapture && !CapturePath.IsValid() && CurrentCaptor.IsValid())
	{
		CapturePath = BuildCapturePathToWidget(CurrentCaptor.ToSharedRef());
	}
	if (bOwnsCapture && !CapturePath.IsValid())
	{
		const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin().IsValid() ? CaptureWidget.Pin() : CurrentCaptor;
		if (PinnedCaptureWidget.IsValid())
		{
			SlateApp.GeneratePathToWidgetUnchecked(PinnedCaptureWidget.ToSharedRef(), CapturePath, EVisibility::Visible);
		}
	}
	if (bOwnsCapture && !CapturePath.IsValid() && CurrentCaptor.IsValid())
	{
		SlateApp.GeneratePathToWidgetUnchecked(CurrentCaptor.ToSharedRef(), CapturePath, EVisibility::Visible);
	}
	const bool bHasValidPath = CapturePath.IsValid();

	if (bOwnsCapture && bHasValidPath)
	{
		const FReply Reply = FReply::Handled()
			.ReleaseMouseCapture();
		SlateApp.ProcessExternalReply(CapturePath, Reply, ActiveUserIndex, ActivePointerIndex);
		SlateApp.QueryCursor();
		UE_LOG(LogValueLadder, Display, TEXT("[Input] EndSlateCapture released gesture=%llu ownsCapture=%s pathValid=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasValidPath ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
	}
	else
	{
		if (SlateUser.IsValid())
		{
			SlateUser->ReleaseCapture(ToSlatePointerIndex(ActivePointerIndex));
			SlateUser->ReleaseCursorCapture();
			SlateApp.QueryCursor();
			UE_LOG(LogValueLadder, Warning, TEXT("[Input] EndSlateCapture used direct SlateUser fallback for gesture=%llu because ownership/path validation failed. ownsCapture=%s pathValid=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasValidPath ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
		}
		else
		{
			UE_LOG(LogValueLadder, Warning, TEXT("[Input] EndSlateCapture skipped release for gesture=%llu because ownership/path validation failed and no SlateUser was available. ownsCapture=%s pathValid=%s user=%d pointer=%d expectedCaptor=%p currentCaptor=%p"), ActiveGestureId, bOwnsCapture ? TEXT("true") : TEXT("false"), bHasValidPath ? TEXT("true") : TEXT("false"), ActiveUserIndex, ActivePointerIndex, CaptureWidget.Pin().Get(), CurrentCaptor.Get());
		}
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

	if (SlateUser->DoesWidgetHaveCapture(PinnedCaptureWidget, ToSlatePointerIndex(ActivePointerIndex)))
	{
		return true;
	}

	return DoesCaptorMatchExpectedWidget(SlateUser->GetPointerCaptor(ToSlatePointerIndex(ActivePointerIndex)));
}

FWidgetPath FValueLadderInputProcessor::BuildCapturePath() const
{
	const TSharedPtr<SWidget> PinnedCaptureWidget = CaptureWidget.Pin();
	return PinnedCaptureWidget.IsValid()
		? BuildCapturePathToWidget(PinnedCaptureWidget.ToSharedRef())
		: FWidgetPath();
}

FWidgetPath FValueLadderInputProcessor::BuildCapturePathToWidget(const TSharedRef<SWidget>& Widget) const
{
	if (!GestureEventPath.IsValid() || !GestureEventPath.ContainsWidget(&Widget.Get()))
	{
		return FWidgetPath();
	}

	return GestureEventPath.GetPathDownTo(Widget);
}

#undef LOCTEXT_NAMESPACE
