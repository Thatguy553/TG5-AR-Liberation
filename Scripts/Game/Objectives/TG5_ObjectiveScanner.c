//------------------------------------------------------------------------------------------------
// Plain description of one discovered objective. The authority produces these
// by scanning the world; clients receive them over the wire. Both sides build
// their TG5_ObjectiveObject list from an identical sequence of defs, which is
// what makes the objective index usable as a network identifier.
class TG5_ObjectiveDef
{
	vector m_vPos;
	string m_sType;
	string m_sName;

	void TG5_ObjectiveDef(vector pos = vector.Zero, string type = "", string name = "")
	{
		m_vPos = pos;
		m_sType = type;
		m_sName = name;
	}
}

//------------------------------------------------------------------------------------------------
// Discovers objectives from the world's map descriptors so the mission works
// on any terrain without hand-placed data. Authority only - the result is
// replicated, so clients never pay for the scan and can never disagree with
// the server about which objectives exist or what order they are in.
class TG5_ObjectiveScanner
{
	static const string OBJ_TYPE_CITY = "city";
	static const string OBJ_TYPE_TOWN = "town";
	static const string OBJ_TYPE_MILITARY = "military";
	static const string OBJ_TYPE_RADIO = "radio";
	static const string OBJ_TYPE_FACTORY = "factory";

	// Radius searched around a generic map descriptor to work out what it is
	protected static const float CLASSIFY_RADIUS = 300;

	protected static const int CITY_BUILDING_COUNT = 13;
	protected static const int TOWN_BUILDING_COUNT = 4;
	protected static const int MILITARY_BUILDING_COUNT = 3;

	// Military assets live under Structures/Military in the vanilla asset tree
	protected static const string MILITARY_PREFAB_TOKEN = "/Military/";

	protected ref array<MapItem> m_aItems = new array<MapItem>();

	// Per-location tallies filled by the CountBuilding query callback
	protected int m_iCivilianBuildings;
	protected int m_iMilitaryBuildings;

	//------------------------------------------------------------------------------------------------
	void Scan(notnull array<ref TG5_ObjectiveDef> outDefs)
	{
		MapEntity mapManager = GetGame().GetMapManager();
		if (!mapManager)
			return;

		CollectByType(mapManager, EMapDescriptorType.MDT_NAME_CITY, OBJ_TYPE_CITY, outDefs);

		// Villages and towns are the same objective type; town is the main one
		CollectByType(mapManager, EMapDescriptorType.MDT_NAME_TOWN, OBJ_TYPE_TOWN, outDefs);
		CollectByType(mapManager, EMapDescriptorType.MDT_NAME_VILLAGE, OBJ_TYPE_TOWN, outDefs);

		ClassifyGenericLocations(mapManager, outDefs);
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectByType(notnull MapEntity mapManager, EMapDescriptorType descriptorType, string objType, notnull array<ref TG5_ObjectiveDef> outDefs)
	{
		m_aItems.Clear();
		mapManager.GetByType(m_aItems, descriptorType);

		foreach (MapItem item : m_aItems)
		{
			outDefs.Insert(new TG5_ObjectiveDef(GroundPos(item.GetPos()), objType, item.GetDisplayName()));
		}
	}

	//------------------------------------------------------------------------------------------------
	// MDT_NAME_GENERIC covers everything the terrain author didn't tag as a
	// city/town/village - military compounds, hamlets, landmarks. Classify by
	// what is actually built there rather than by the display name, which is
	// localised UI text.
	protected void ClassifyGenericLocations(notnull MapEntity mapManager, notnull array<ref TG5_ObjectiveDef> outDefs)
	{
		m_aItems.Clear();
		mapManager.GetByType(m_aItems, EMapDescriptorType.MDT_NAME_GENERIC);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		foreach (MapItem item : m_aItems)
		{
			m_iCivilianBuildings = 0;
			m_iMilitaryBuildings = 0;

			vector pos = item.GetPos();

			// STATIC | WITH_OBJECT skips characters, vehicles and everything
			// without a mesh, which is the bulk of a 300 m sphere
			world.QueryEntitiesBySphere(pos, CLASSIFY_RADIUS, CountBuilding, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.WITH_OBJECT);

			string objType = ClassifyCounts(m_iCivilianBuildings, m_iMilitaryBuildings);
			if (objType.IsEmpty())
				continue;

			outDefs.Insert(new TG5_ObjectiveDef(GroundPos(pos), objType, item.GetDisplayName()));
		}
	}

	//------------------------------------------------------------------------------------------------
	// Returns an empty string for locations too small to be worth capturing.
	protected string ClassifyCounts(int civilian, int military)
	{
		// A handful of military structures outweighs the odd farmhouse next door
		if (military >= MILITARY_BUILDING_COUNT && military * 2 >= civilian)
			return OBJ_TYPE_MILITARY;

		int total = civilian + military;

		if (total >= CITY_BUILDING_COUNT)
			return OBJ_TYPE_CITY;

		if (total >= TOWN_BUILDING_COUNT)
			return OBJ_TYPE_TOWN;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CountBuilding(IEntity ent)
	{
		string prefab = SCR_ResourceNameUtils.GetPrefabName(ent);

		if (!prefab.IsEmpty() && prefab.Contains(MILITARY_PREFAB_TOKEN))
		{
			m_iMilitaryBuildings++;
			return true;
		}

		if (SCR_DestructibleBuildingEntity.Cast(ent))
			m_iCivilianBuildings++;

		return true; // continue the query
	}

	//------------------------------------------------------------------------------------------------
	// Map descriptors carry no meaningful height, so drop the position onto the
	// terrain - triggers are spheres and would otherwise sit under or above it.
	protected vector GroundPos(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world)
			pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		return pos;
	}
}
