//------------------------------------------------------------------------------------------------
class TG5_MapObjectiveInit : SCR_MapUIBaseComponent
{
	// Objective type identifiers - used as the key for icon lookup in GetIconForType
	static const string OBJ_TYPE_CITY = "city";
	static const string OBJ_TYPE_VILLAGE = "village";
	static const string OBJ_TYPE_MILITARY = "military";
	static const string OBJ_TYPE_RADIO = "radio";
	static const string OBJ_TYPE_FACTORY = "factory";

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

	// Set false to silence diagnostic output
	protected bool m_bDebugLog = true;

	//------------------------------------------------------------------------------------------------
	// Objective source data (gathered once, filtered by type)
	//------------------------------------------------------------------------------------------------
	protected ref array<MapItem> m_aMapCities = new array<MapItem>();
	protected ref array<MapItem> m_aMapTowns = new array<MapItem>();
	protected ref array<MapItem> m_aMapVillages = new array<MapItem>();
	protected ref array<MapItem> m_aMapMilitary = new array<MapItem>();
	protected ref array<MapItem> m_aMapGenerics = new array<MapItem>();

	// Full session objective list - accumulated across every AddObjectiveMark call,
	// used to rebuild markers on every map open for the rest of the session.
	// Each TG5_ObjectiveObject owns its runtime widget while the map is open.
	protected ref array<ref TG5_ObjectiveObject> m_aAllObjectives = new array<ref TG5_ObjectiveObject>();

	//------------------------------------------------------------------------------------------------
	// Marker widget runtime state
	//------------------------------------------------------------------------------------------------
	protected bool m_bMapViewEventsSubscribed = false;
	protected bool m_bOpenSubscribed = false;

	// Scratch array reused by FilterGenericLocations' sphere query
	protected ref array<IEntity> m_aBuildingEntities = new array<IEntity>();

	//------------------------------------------------------------------------------------------------
	// Init
	//------------------------------------------------------------------------------------------------

	// Call once the world/game mode is confirmed ready (e.g. from your game mode's
	// OnWorldPostProcess) - NOT from the constructor, since the map/world systems
	// this depends on aren't guaranteed to exist yet at construction time.
	void Initialize()
	{
		GatherMapLocations();
		AddObjectiveMark(m_aMapCities, OBJ_TYPE_CITY);
		AddObjectiveMark(m_aMapVillages, OBJ_TYPE_VILLAGE);
		AddObjectiveMark(m_aMapMilitary, OBJ_TYPE_MILITARY);
	}

	//------------------------------------------------------------------------------------------------
	// Authority-safe variant: runs the same map descriptor gathering and
	// classification WITHOUT creating markers, widgets, or subscribing to any
	// map UI events. Used by the objective manager on dedicated servers, which
	// need the objective list (positions/types) but have no map UI.
	void GatherOnly()
	{
		GatherMapLocations();

		BuildObjectiveObjects(m_aMapCities, OBJ_TYPE_CITY);
		BuildObjectiveObjects(m_aMapVillages, OBJ_TYPE_VILLAGE);
		BuildObjectiveObjects(m_aMapMilitary, OBJ_TYPE_MILITARY);
	}

	//------------------------------------------------------------------------------------------------
	// Public API
	//------------------------------------------------------------------------------------------------

	// Batch-add objectives gathered from the map descriptor system
	void AddObjectiveMark(array<MapItem> objectives, string objType)
	{
		if (!objectives || objectives.IsEmpty())
			return;

		array<ref TG5_ObjectiveObject> batch = BuildObjectiveObjects(objectives, objType);

		EnsureOpenSubscription();

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			BuildMarkers(batch); // map already open - build just this batch
	}

	//------------------------------------------------------------------------------------------------
	// Wrap map items in objective objects and register them in the session
	// list. No UI work - safe to call on a dedicated server.
	protected array<ref TG5_ObjectiveObject> BuildObjectiveObjects(array<MapItem> objectives, string objType)
	{
		array<ref TG5_ObjectiveObject> batch = new array<ref TG5_ObjectiveObject>();

		foreach (MapItem item : objectives)
		{
			TG5_ObjectiveObject obj = new TG5_ObjectiveObject();
			obj.SetObjType(objType);
			obj.SetObjMapItem(item);
			obj.SetObjEntity(item.Entity()); // may be null for pure name-descriptor items
			
			TG5_ObjectiveTriggerEntity trig = BuildObjectiveTrigger(obj);
			obj.SetTrigger(trig);
			obj.BindTrigger();
			
			m_aAllObjectives.Insert(obj);
			batch.Insert(obj);
		}

		return batch;
	}
	
	//------------------------------------------------------------------------------------------------
	// Spawn the trigger entity and return it
	// 
	protected TG5_ObjectiveTriggerEntity BuildObjectiveTrigger(TG5_ObjectiveObject obj)
	{
		vector pos = GetObjectivePos(obj); // MapItem pos, falling back to entity origin
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		
		ResourceName trigger = "{D9130D20F5A6942F}Prefabs/Triggers/TG5_ObjectiveTriggerEntity.et";
		
		IEntity ent = GetGame().SpawnEntityPrefabEx(trigger, false, null, params: params);
		return TG5_ObjectiveTriggerEntity.Cast(ent);
	}

	//------------------------------------------------------------------------------------------------
	// Add a single pre-built objective - for runtime/side-mission objectives that
	// have an entity (or just a position) rather than a map descriptor item
	void AddObjectiveMark(TG5_ObjectiveObject obj)
	{
		if (!obj)
			return;

		m_aAllObjectives.Insert(obj);
		EnsureOpenSubscription();

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			BuildMarker(obj);
			SubscribeMapViewEvents();
		}
	}

	//------------------------------------------------------------------------------------------------
	void RemoveObjectiveMark(TG5_ObjectiveObject obj)
	{
		if (!obj)
			return;

		Widget w = obj.GetObjWidget();
		if (w)
		{
			w.RemoveFromHierarchy();
			obj.SetObjWidget(null);
		}

		m_aAllObjectives.RemoveItem(obj);
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
	array<ref TG5_ObjectiveObject> GetObjectives()
	{
		return m_aAllObjectives;
	}

	//------------------------------------------------------------------------------------------------
	// Marker building / positioning
	//------------------------------------------------------------------------------------------------

	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		if (!m_aAllObjectives.IsEmpty())
			RebuildMarkers();
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildMarkers()
	{
		ClearMarkerWidgets();

		foreach (TG5_ObjectiveObject obj : m_aAllObjectives)
		{
			BuildMarker(obj);
		}

		SubscribeMapViewEvents();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildMarkers(array<ref TG5_ObjectiveObject> objectives)
	{
		foreach (TG5_ObjectiveObject obj : objectives)
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

		vector worldPos = GetObjectivePos(obj);

		TextWidget objName = TextWidget.Cast(w.FindAnyWidget("Name"));
		if (objName && obj.GetObjMapItem())
			objName.SetText(obj.GetObjMapItem().GetDisplayName());

		ImageWidget objImage = ImageWidget.Cast(w.FindAnyWidget("Image0"));
		if (objImage)
		{
			objImage.LoadImageFromSet(0, m_sImageSet, GetIconForType(obj.GetObjType()));
			objImage.SetImage(0);
			objImage.SetColor(Color.DarkRed);
		}

		PositionMarker(w, worldPos);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMarkerWidgets()
	{
		// Widgets live inside the map frame; if the map is open they still exist
		// and must be removed explicitly, otherwise closing the map destroys them
		foreach (TG5_ObjectiveObject obj : m_aAllObjectives)
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
	// Objectives are positioned by their map item when available, falling back
	// to their entity origin (entity-based runtime objectives)
	protected vector GetObjectivePos(TG5_ObjectiveObject obj)
	{
		if (obj.GetObjMapItem())
			return obj.GetObjMapItem().GetPos();

		if (obj.GetObjEntity())
			return obj.GetObjEntity().GetOrigin();

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetIconForType(string objType)
	{
		switch (objType)
		{
			case OBJ_TYPE_CITY: return m_sImgCity;
			case OBJ_TYPE_VILLAGE: return m_sImgTown;
			case OBJ_TYPE_MILITARY: return m_sImgMilitary;
			case OBJ_TYPE_RADIO: return m_sImgRadio;
			case OBJ_TYPE_FACTORY: return m_sImgFactory;
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
		foreach (TG5_ObjectiveObject obj : m_aAllObjectives)
		{
			Widget w = obj.GetObjWidget();
			if (w)
				PositionMarker(w, GetObjectivePos(obj));
		}
	}

	//------------------------------------------------------------------------------------------------
	// Map view event handlers
	//------------------------------------------------------------------------------------------------

	// Subscribe once for the whole session; OnMapOpen rebuilds from the
	// full m_aAllObjectives list every time the map opens from here on.
	// m_bOpenSubscribed is set HERE at subscription time and never reset on
	// map close - the invoker survives across opens, so the handler must not
	// be inserted again (ScriptInvoker.Insert does not deduplicate).
	protected void EnsureOpenSubscription()
	{
		if (m_bOpenSubscribed)
			return;

		SCR_MapEntity.GetOnMapOpenComplete().Insert(OnMapOpen);
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
	// drop our references and unsubscribe until OnMapOpen rebuilds next time
	protected void OnMapClosed(MapConfiguration config)
	{
		foreach (TG5_ObjectiveObject obj : m_aAllObjectives)
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
	// Objective gathering / filtering
	//------------------------------------------------------------------------------------------------

	protected void GatherMapLocations()
	{
		DebugLog("[MapObjectiveInit] GatherMapLocations Running");

		MapEntity mapEntity = GetGame().GetMapManager();
		if (mapEntity)
		{
			int count = mapEntity.GetByType(m_aMapCities, EMapDescriptorType.MDT_NAME_CITY);
			count += mapEntity.GetByType(m_aMapTowns, EMapDescriptorType.MDT_NAME_TOWN);
			count += mapEntity.GetByType(m_aMapVillages, EMapDescriptorType.MDT_NAME_VILLAGE);
			count += mapEntity.GetByType(m_aMapGenerics, EMapDescriptorType.MDT_NAME_GENERIC);
			DebugLog(count.ToString());
		}

		m_aMapCities.InsertAll(m_aMapTowns);

		array<MapItem> filteredCity = new array<MapItem>();
		array<MapItem> filteredVillage = new array<MapItem>();
		array<MapItem> filteredMilitary = new array<MapItem>();
		FilterGenericLocations(mapEntity, m_aMapGenerics, filteredCity, filteredVillage, filteredMilitary);

		m_aMapCities.InsertAll(filteredCity);
		m_aMapVillages.InsertAll(filteredVillage);
		m_aMapMilitary.InsertAll(filteredMilitary);

		DebugLog(string.Format("[GatherMapLocations] Cities: %1 | Villages: %2 | Military: %3",
			m_aMapCities.Count(), m_aMapVillages.Count(), m_aMapMilitary.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void FilterGenericLocations(MapEntity mapEnt, array<MapItem> mapItems, out array<MapItem> city, out array<MapItem> village, out array<MapItem> military)
	{
		float searchRadius = 300.0;

		foreach (MapItem item : mapItems)
		{
			m_aBuildingEntities.Clear(); // clear at the START of every iteration - guarantees no leakage regardless of which branch below fires

			string mapName = item.GetDisplayName();
			DebugLog("[Filter] Checking Location: " + mapName);

			GetGame().GetWorld().QueryEntitiesBySphere(item.GetPos(), searchRadius, AddBuildingEntity, null, EQueryEntitiesFlags.ALL);

			if (mapName.Contains("Military"))
			{
				DebugLog("[Filter] Military Inserted");
				military.Insert(item);
				continue;
			}

			int buildingCount = m_aBuildingEntities.Count();

			if (buildingCount >= 13)
			{
				DebugLog("[Filter] City Inserted");
				city.Insert(item);
				continue;
			}

			if (buildingCount >= 4 && buildingCount <= 10)
			{
				DebugLog("[Filter] Village Inserted");
				village.Insert(item);
				continue;
			}

			DebugLog("[Filter] Not Inserted");
		}
	}

	//------------------------------------------------------------------------------------------------
	// Sphere query callback - only destructible buildings count toward the
	// city/village classification; trees, vehicles, characters etc. are ignored
	bool AddBuildingEntity(IEntity ent)
	{
		if (SCR_DestructibleBuildingEntity.Cast(ent))
			m_aBuildingEntities.Insert(ent);

		return true; // continue the query
	}

	//------------------------------------------------------------------------------------------------
	// Cleanup
	//------------------------------------------------------------------------------------------------

	// Unsubscribe everything - call from the owning component's destructor.
	// The invokers are static and outlive this object; leaving handlers bound
	// would keep this instance alive and firing against a dead context.
	void Cleanup()
	{
		if (m_bOpenSubscribed)
		{
			SCR_MapEntity.GetOnMapOpenComplete().Remove(OnMapOpen);
			m_bOpenSubscribed = false;
		}

		if (m_bMapViewEventsSubscribed)
			OnMapClosed(null);

		m_aAllObjectives.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void DebugLog(string msg)
	{
		if (m_bDebugLog)
			Print(msg);
	}

	//------------------------------------------------------------------------------------------------
	// Future / not yet implemented
	//------------------------------------------------------------------------------------------------

	protected void UpdateObjectiveMarkName()
	{
	}
}
