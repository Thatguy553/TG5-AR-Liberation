//------------------------------------------------------------------------------------------------
// Handles capture progress UI and notifications for objectives.
// Creates and manages progress bars when objectives are being captured,
// and shows notification popups when objectives are captured/lost.
class TG5_CaptureUIComponent
{
	//------------------------------------------------------------------------------------------------
	// Config
	//------------------------------------------------------------------------------------------------
	protected ResourceName m_sCaptureProgressLayout = "{1EDA099FC2EEA111}UI/layouts/Map/TG5_ObjCaptureProgress.layout";
	protected ResourceName m_sCapturedLayout = "{46E1BBB6F80185A5}UI/layouts/Map/TG5_ObjCaptured.layout";
	protected ResourceName m_sLostLayout = "{AA4F47D551A346C3}UI/layouts/Map/TG5_ObjLost.layout";

	protected float m_fNotificationDuration = 5.0;  // Seconds to show notification

	//------------------------------------------------------------------------------------------------
	protected TG5_ObjectiveManagerComponent m_Manager;

	protected bool m_bSubscribed = false;

	//------------------------------------------------------------------------------------------------
	void TG5_CaptureUIComponent(notnull TG5_ObjectiveManagerComponent manager)
	{
		m_Manager = manager;

		m_Manager.GetOnObjectivesReady().Insert(OnObjectivesReady);
		m_Manager.GetOnCaptureProgress().Insert(OnCaptureProgress);
		m_Manager.GetOnObjectiveCaptured().Insert(OnObjectiveCaptured);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnObjectivesReady()
	{
		if (!m_bSubscribed)
		{
			m_bSubscribed = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref TG5_ObjectiveObject> GetObjectives()
	{
		return m_Manager.GetObjectives();
	}

	//------------------------------------------------------------------------------------------------
	// Handle capture progress updates - show/hide progress bars
	protected void OnCaptureProgress(TG5_ObjectiveObject obj, float progress, FactionKey capturingFaction)
	{
		if (!obj)
			return;

		// If not being captured, remove progress bar
		if (!obj.IsBeingCaptured() || progress <= 0.0)
		{
			RemoveCaptureProgress(obj);
			return;
		}

		// Show or update progress bar
		UpdateCaptureProgress(obj, progress, capturingFaction);
	}

	//------------------------------------------------------------------------------------------------
	// Create or update the capture progress bar for an objective
	protected void UpdateCaptureProgress(TG5_ObjectiveObject obj, float progress, FactionKey capturingFaction)
	{
		Widget progressWidget = obj.GetCaptureProgressWidget();
		
		// Create widget if it doesn't exist
		if (!progressWidget)
		{
			progressWidget = CreateCaptureProgressWidget(obj);
			if (!progressWidget)
				return;
			
			obj.SetCaptureProgressWidget(progressWidget);
		}

		// Update progress bar fill
		ProgressBarWidget progressBar = ProgressBarWidget.Cast(progressWidget.FindAnyWidget("ProgressBarTop"));
		if (progressBar)
		{
			progressBar.SetCurrent(progress);
		}

		// Update objective name
		RichTextWidget objName = RichTextWidget.Cast(progressWidget.FindAnyWidget("ObjName"));
		if (objName)
		{
			objName.SetText(obj.GetDisplayName());
		}

		// Color based on capturing faction
		ImageWidget progressBarBot = ImageWidget.Cast(progressWidget.FindAnyWidget("ProgressBarBot"));
		if (progressBarBot)
		{
			Color factionColor = GetFactionColor(capturingFaction);
			progressBarBot.SetColor(factionColor);
		}

		// Also update the progress bar fill color
		ProgressBarWidget progressBar = ProgressBarWidget.Cast(progressWidget.FindAnyWidget("ProgressBarTop"));
		if (progressBar)
		{
			Color factionColor = GetFactionColor(capturingFaction);
			progressBar.SetColor(factionColor);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Create the capture progress widget
	protected Widget CreateCaptureProgressWidget(TG5_ObjectiveObject obj)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget progressWidget = workspace.CreateWidgets(m_sCaptureProgressLayout);
		if (!progressWidget)
			return null;

		// Position in bottom-right corner of screen as HUD element
		FrameSlot.SetAnchorMin(progressWidget, 1, 1);
		FrameSlot.SetAnchorMax(progressWidget, 1, 1);
		FrameSlot.SetAlignment(progressWidget, 1, 1);
		
		// Set position with some padding from screen edges
		int screenWidth, screenHeight;
		workspace.GetScreenSize(screenWidth, screenHeight);
		
		float paddingX = 20.0;  // Padding from right edge
		float paddingY = 100.0; // Padding from bottom edge
		
		FrameSlot.SetPos(progressWidget, -paddingX, -paddingY);

		return progressWidget;
	}

	//------------------------------------------------------------------------------------------------
	// Remove the capture progress widget
	protected void RemoveCaptureProgress(TG5_ObjectiveObject obj)
	{
		Widget progressWidget = obj.GetCaptureProgressWidget();
		if (progressWidget)
		{
			progressWidget.RemoveFromHierarchy();
			obj.SetCaptureProgressWidget(null);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Handle objective capture - show notification
	protected void OnObjectiveCaptured(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner)
	{
		if (!obj)
			return;

		// Remove progress bar if present
		RemoveCaptureProgress(obj);

		// Show notification based on whether we gained or lost the objective
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;

		Faction playerFaction = factionManager.GetPlayerFaction();
		if (!playerFaction)
			return;

		FactionKey playerFactionKey = playerFaction.GetFactionKey();

		if (newOwner == playerFactionKey)
		{
			ShowCapturedNotification(obj);
		}
		else if (oldOwner == playerFactionKey)
		{
			ShowLostNotification(obj);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Show "Sector Captured" notification
	protected void ShowCapturedNotification(TG5_ObjectiveObject obj)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget notificationWidget = workspace.CreateWidgets(m_sCapturedLayout);
		if (!notificationWidget)
			return;

		obj.SetNotificationWidget(notificationWidget);

		// Update message with objective name
		RichTextWidget message = RichTextWidget.Cast(notificationWidget.FindAnyWidget("Message"));
		if (message)
		{
			string formattedText = string.Format("Our forces have captured %1", obj.GetDisplayName());
			message.SetText(formattedText);
		}

		// Auto-hide after duration
		GetGame().GetCallqueue().CallLater(HideNotification, m_fNotificationDuration * 1000, false, obj);
	}

	//------------------------------------------------------------------------------------------------
	// Show "Sector Lost" notification
	protected void ShowLostNotification(TG5_ObjectiveObject obj)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget notificationWidget = workspace.CreateWidgets(m_sLostLayout);
		if (!notificationWidget)
			return;

		obj.SetNotificationWidget(notificationWidget);

		// Update message with objective name
		RichTextWidget message = RichTextWidget.Cast(notificationWidget.FindAnyWidget("Message"));
		if (message)
		{
			string formattedText = string.Format("Enemy forces have captured %1", obj.GetDisplayName());
			message.SetText(formattedText);
		}

		// Auto-hide after duration
		GetGame().GetCallqueue().CallLater(HideNotification, m_fNotificationDuration * 1000, false, obj);
	}

	//------------------------------------------------------------------------------------------------
	// Hide and remove notification widget
	protected void HideNotification(TG5_ObjectiveObject obj)
	{
		if (!obj)
			return;

		Widget notificationWidget = obj.GetNotificationWidget();
		if (notificationWidget)
		{
			notificationWidget.RemoveFromHierarchy();
			obj.SetNotificationWidget(null);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Get faction color for UI elements
	protected Color GetFactionColor(FactionKey factionKey)
	{
		if (factionKey.IsEmpty())
			return Color.White;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return Color.White;

		Faction faction = factionManager.GetFactionByKey(factionKey);
		if (!faction)
			return Color.White;

		return faction.GetFactionColor();
	}

	//------------------------------------------------------------------------------------------------
	// Cleanup
	void Cleanup()
	{
		// Remove all progress bars and notifications
		foreach (TG5_ObjectiveObject obj : GetObjectives())
		{
			RemoveCaptureProgress(obj);
			HideNotification(obj);
		}

		// Unsubscribe from events
		if (m_Manager)
		{
			m_Manager.GetOnObjectivesReady().Remove(OnObjectivesReady);
			m_Manager.GetOnCaptureProgress().Remove(OnCaptureProgress);
			m_Manager.GetOnObjectiveCaptured().Remove(OnObjectiveCaptured);
		}

		m_bSubscribed = false;
	}

	//------------------------------------------------------------------------------------------------
	void ~TG5_CaptureUIComponent()
	{
		Cleanup();
	}
}