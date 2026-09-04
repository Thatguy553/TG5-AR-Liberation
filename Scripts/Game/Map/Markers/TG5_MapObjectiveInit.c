//------------------------------------------------------------------------------------------------
// Draws an objective marker on the map for every objective the manager knows
// about. Purely presentational: it discovers nothing, owns no objective state
// and spawns no entities, so it is safe for this to not exist at all (which is
// the case on a dedicated server).
//
// Deliberately not a SCR_MapUIBaseComponent. That base class is constructed by
// SCR_MapEntity from the map configuration's component list and driven through
// Init/SetActive/OnMapOpen; instantiating one by hand gets none of that
// lifecycle, and its constructor dereferences a map instance that a dedicated
// server does not have. Registering a real map UI component (or moving to map
// descriptors) is the longer-term fix.
class TG5_MapObjectiveInit
{
	//------------------------------------------------------------------------------------------------
	// Config
	//------------------------------------------------------------------------------------------------
	protected ResourceName m_sObjectiveLayout = "{A26AA5835700705D}UI/layouts/Campaign/TG5_ObjectiveMarker.layout";

	// Imageset containing all the objective icons for the map
	protected ResourceName m_sImageSet = "{C920BEB3057D4187}UI/Imagesets/TG5_ObjTypes.imageset";
	protected string m_sImgCity = "city";
	protected string m_sImgMilitary = "military";
	protected string m_sImgTown = "town";
	protected string m_sImgRadio = "radio";
	protected string m_sImgFactory = "factory";


	//------------------------------------------------------------------------------------------------
	protected TG5_ObjectiveManagerComponent m_Manager;

	protected bool m_bMapViewEventsSubscribed = false;
	protected bool m_bOpenSubscribed = false;

	//------------------------------------------------------------------------------------------------
	void TG5_MapObjectiveInit(notnull TG5_ObjectiveManagerComponent manager)
	{
		m_Manager = manager;

		m_Manager.GetOnObjectivesReady().Insert(OnObjectivesReady);
		m_Manager.GetOnObjectiveCaptured().Insert(OnObjectiveCaptured);
	}

	//------------------------------------------------------------------------------------------------
	// The objective list arrives from the authority (or is built locally on a
	// listen server) after this component exists, so markers are built here
	// rather than at construction.
	protected void OnObjectivesReady()
	{
		EnsureOpenSubscription();

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			RebuildMarkers();
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref TG5_ObjectiveObject> GetObjectives()
	{
		return m_Manager.GetObjectives();
	}

	//------------------------------------------------------------------------------------------------
	// Marker building / positioning
	//------------------------------------------------------------------------------------------------

	protected void OnMapOpenComplete(MapConfiguration config)
	{
		if (!GetObjectives().IsEmpty())
			RebuildMarkers();
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildMarkers()
	{
		ClearMarkerWidgets();

		foreach (TG5_ObjectiveObject obj : GetObjectives())
		{
			BuildMarker(obj);
		}

		SubscribeMapViewEvents();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildMarker(TG5_ObjectiveObject obj)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.GetMapMenuRoot())
			return;

		Widget mapFrame = mapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;

		Widget w = GetGame().GetWorkspace().CreateWidgets(m_sObjectiveLayout, mapFrame);
		if (!w)
			return;

		FrameSlot.SetAnchorMin(w, 0, 0);
		FrameSlot.SetAnchorMax(w, 0, 0);
		FrameSlot.SetAlignment(w, 0.5, 0.5);
		FrameSlot.SetSizeToContent(w, true);

		obj.SetObjWidget(w);

		TextWidget objName = TextWidget.Cast(w.FindAnyWidget("Name"));
		if (objName)
			objName.SetText(obj.GetDisplayName());

		ImageWidget objImage = ImageWidget.Cast(w.FindAnyWidget("Image0"));
		if (objImage)
		{
			objImage.LoadImageFromSet(0, m_sImageSet, GetIconForType(obj.GetObjType()));
			objImage.SetImage(0);
		}

		ApplyOwnerColor(obj);
		PositionMarker(w, obj.GetPos());
	}

	//------------------------------------------------------------------------------------------------
	// Fired on every machine by the manager's capture broadcast
	protected void OnObjectiveCaptured(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner)
	{
		ApplyOwnerColor(obj);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOwnerColor(TG5_ObjectiveObject obj)
	{
		Widget w = obj.GetObjWidget();
		if (!w)
			return;

		ImageWidget objImage = ImageWidget.Cast(w.FindAnyWidget("Image0"));
		if (!objImage)
			return;

		objImage.SetColor(GetOwnerColor(obj.GetOwningFaction()));
	}

	//------------------------------------------------------------------------------------------------
	// Unowned objectives keep the original dark red
	protected Color GetOwnerColor(FactionKey owner)
	{
		if (owner.IsEmpty())
			return Color.DarkRed;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return Color.DarkRed;

		Faction faction = factionManager.GetFactionByKey(owner);
		if (!faction)
			return Color.DarkRed;

		return faction.GetFactionColor();
	}

	//------------------------------------------------------------------------------------------------
	// Change an objective's type - swaps the icon immediately if the marker is drawn
	void FlipObjectiveMark(TG5_ObjectiveObject obj, string newType)
	{
		if (!obj)
			return;

		obj.SetObjType(newType);

		Widget w = obj.GetObjWidget();
		if (!w)
			return;

		ImageWidget objImage = ImageWidget.Cast(w.FindAnyWidget("Image0"));
		if (objImage)
		{
			objImage.LoadImageFromSet(0, m_sImageSet, GetIconForType(newType));
			objImage.SetImage(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMarkerWidgets()
	{
		// Widgets live inside the map frame; if the map is open they still exist
		// and must be removed explicitly, otherwise closing the map destroys them
		foreach (TG5_ObjectiveObject obj : GetObjectives())
		{
			Widget w = obj.GetObjWidget();
			if (w)
			{
				w.RemoveFromHierarchy();
				obj.SetObjWidget(null);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string GetIconForType(string objType)
	{
		switch (objType)
		{
			case "city": return m_sImgCity;
			case "town": return m_sImgTown;
			case "military": return m_sImgMilitary;
			case "radio": return m_sImgRadio;
			case "factory": return m_sImgFactory;
		}

		return m_sImgTown;
	}

	//------------------------------------------------------------------------------------------------
	protected void PositionMarker(Widget w, vector worldPos)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		int screenX, screenY;
		mapEntity.WorldToScreen(worldPos[0], worldPos[2], screenX, screenY, true);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		FrameSlot.SetPos(w, workspace.DPIUnscale(screenX), workspace.DPIUnscale(screenY));
	}

	//------------------------------------------------------------------------------------------------
	protected void RepositionAll()
	{
		foreach (TG5_ObjectiveObject obj : GetObjectives())
		{
			Widget w = obj.GetObjWidget();
			if (w)
				PositionMarker(w, obj.GetPos());
		}
	}

	//------------------------------------------------------------------------------------------------
	// Map view event handlers
	//------------------------------------------------------------------------------------------------

	// Subscribe once for the whole session; OnMapOpenComplete rebuilds from the
	// manager's objective list every time the map opens from here on.
	// m_bOpenSubscribed is set HERE at subscription time and never reset on
	// map close - the invoker survives across opens, so the handler must not
	// be inserted again (ScriptInvoker.Insert does not deduplicate).
	protected void EnsureOpenSubscription()
	{
		if (m_bOpenSubscribed)
			return;

		SCR_MapEntity.GetOnMapOpenComplete().Insert(OnMapOpenComplete);
		m_bOpenSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SubscribeMapViewEvents()
	{
		if (m_bMapViewEventsSubscribed)
			return;

		SCR_MapEntity.GetOnMapPan().Insert(OnMapViewChanged);
		SCR_MapEntity.GetOnMapZoom().Insert(OnMapZoomChanged);
		SCR_MapEntity.GetOnLayerChanged().Insert(OnMapLayerChanged);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClosed);
		m_bMapViewEventsSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnMapViewChanged(float panX, float panY, bool userInitiated)
	{
		RepositionAll();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoomChanged(float zoomLevel)
	{
		RepositionAll();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapLayerChanged(int layer)
	{
		RepositionAll();
	}

	//------------------------------------------------------------------------------------------------
	// Map's internal widgets get torn down on close, so ours go with them -
	// drop our references and unsubscribe until the map opens again
	protected void OnMapClosed(MapConfiguration config)
	{
		foreach (TG5_ObjectiveObject obj : GetObjectives())
		{
			obj.SetObjWidget(null);
		}

		SCR_MapEntity.GetOnMapPan().Remove(OnMapViewChanged);
		SCR_MapEntity.GetOnMapZoom().Remove(OnMapZoomChanged);
		SCR_MapEntity.GetOnLayerChanged().Remove(OnMapLayerChanged);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClosed);
		m_bMapViewEventsSubscribed = false;
	}

	//------------------------------------------------------------------------------------------------
	// Cleanup
	//------------------------------------------------------------------------------------------------

	// Unsubscribe everything - call from the owning component's destructor.
	// The map invokers are static and outlive this object; leaving handlers
	// bound would keep this instance alive and firing against a dead context.
	void Cleanup()
	{
		if (m_bOpenSubscribed)
		{
			SCR_MapEntity.GetOnMapOpenComplete().Remove(OnMapOpenComplete);
			m_bOpenSubscribed = false;
		}

		if (m_bMapViewEventsSubscribed)
			OnMapClosed(null);

		if (m_Manager)
		{
			m_Manager.GetOnObjectivesReady().Remove(OnObjectivesReady);
			m_Manager.GetOnObjectiveCaptured().Remove(OnObjectiveCaptured);
		}
	}
}
