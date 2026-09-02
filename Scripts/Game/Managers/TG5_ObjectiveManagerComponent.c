void TG5_OnObjectiveCapturedDelegate(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner);
typedef func TG5_OnObjectiveCapturedDelegate;
typedef ScriptInvokerBase<TG5_OnObjectiveCapturedDelegate> TG5_OnObjectiveCapturedInvoker;

void TG5_OnObjectiveContestedDelegate(TG5_ObjectiveObject obj, FactionKey contestingFaction);
typedef func TG5_OnObjectiveContestedDelegate;
typedef ScriptInvokerBase<TG5_OnObjectiveContestedDelegate> TG5_OnObjectiveContestedInvoker;

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Callsign", description: "Authoritative manager for objectives: state, capture logic, and AI garrison spawning.")]
class TG5_ObjectiveManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
// Runs on server AND client. All capture evaluation and AI spawning is
// authority-only (guarded by IsServer). State changes reach clients via
// broadcast RPC, which fires the invokers so client UI can react.
class TG5_ObjectiveManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute(defvalue: "150", desc: "Radius (m) around an objective within which faction presence is evaluated.", category: "Objectives")]
	protected float m_fCaptureRadius;

	[Attribute(defvalue: "2", desc: "Seconds between capture evaluations on the authority.", category: "Objectives", params: "0.5 60 0.5")]
	protected float m_fCaptureCheckInterval;

	[Attribute(defvalue: "{6F72F05752ED62A8}Prefabs/Groups/OPFOR/Group_USSR_FireGroup_Guard.et", desc: "Default AI group prefab used for objective garrisons (authority only).", category: "Objectives", params: "et")]
	protected ResourceName m_sDefaultGarrisonPrefab;

	//------------------------------------------------------------------------------------------------
	protected static TG5_ObjectiveManagerComponent s_Instance;

	protected ref array<ref TG5_ObjectiveObject> m_aObjectives = new array<ref TG5_ObjectiveObject>();

	protected ref TG5_OnObjectiveCapturedInvoker m_OnObjectiveCaptured = new TG5_OnObjectiveCapturedInvoker();
	protected ref TG5_OnObjectiveContestedInvoker m_OnObjectiveContested = new TG5_OnObjectiveContestedInvoker();

	// SCR_MapObjectiveInit is created lazily on clients only; the dedicated
	// server never gets one (it has no map UI).
	protected ref TG5_MapObjectiveInit m_MapObjectives;

	// Scratch arrays reused by the capture evaluation sphere query
	protected ref array<IEntity> m_aQueryResults = new array<IEntity>();
	protected ref array<int> m_aFactionCounts = new array<int>();

	//------------------------------------------------------------------------------------------------
	static TG5_ObjectiveManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	TG5_OnObjectiveCapturedInvoker GetOnObjectiveCaptured()
	{
		return m_OnObjectiveCaptured;
	}

	//------------------------------------------------------------------------------------------------
	TG5_OnObjectiveContestedInvoker GetOnObjectiveContested()
	{
		return m_OnObjectiveContested;
	}

	//------------------------------------------------------------------------------------------------
	TG5_MapObjectiveInit GetMapObjectives()
	{
		return m_MapObjectives;
	}

	//------------------------------------------------------------------------------------------------
	array<ref TG5_ObjectiveObject> GetObjectives()
	{
		return m_aObjectives;
	}

	//------------------------------------------------------------------------------------------------
	// Lifecycle
	//------------------------------------------------------------------------------------------------

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnWorldPostProcess(World world)
	{
		m_MapObjectives = new TG5_MapObjectiveInit();

		if (System.IsConsoleApp())
		{
			// Dedicated server: no map UI exists. Gather the objective list
			// (positions/types) without building any markers or subscribing
			// to map events - the authority still needs objectives to
			// garrison and evaluate captures against.
			m_MapObjectives.GatherOnly();
		}
		else
		{
			// Client / listen server: full init with map markers
			m_MapObjectives.Initialize();
		}

		array<ref TG5_ObjectiveObject> gathered = m_MapObjectives.GetObjectives();
		foreach (TG5_ObjectiveObject obj : gathered)
		{
			RegisterObjective(obj);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		// Capture evaluation and garrison spawning are authority-only
		if (!IsServer())
			return;

		// Garrison every registered objective at game start
		/*foreach (TG5_ObjectiveObject obj : m_aObjectives)
		{
			SpawnGarrison(obj);
		}*/

		GetGame().GetCallqueue().CallLater(EvaluateCaptures, m_fCaptureCheckInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~TG5_ObjectiveManagerComponent()
	{
		if (s_Instance == this)
			s_Instance = null;

		GetGame().GetCallqueue().Remove(EvaluateCaptures);

		if (m_MapObjectives)
			m_MapObjectives.Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	// Registration
	//------------------------------------------------------------------------------------------------

	void RegisterObjective(TG5_ObjectiveObject obj)
	{
		if (!obj || m_aObjectives.Contains(obj))
			return;

		m_aObjectives.Insert(obj);
	}

	//------------------------------------------------------------------------------------------------
	void UnregisterObjective(TG5_ObjectiveObject obj)
	{
		m_aObjectives.RemoveItem(obj);
	}

	//------------------------------------------------------------------------------------------------
	// Capture evaluation (authority only)
	//------------------------------------------------------------------------------------------------

	protected bool IsServer()
	{
		if (!GetGame())
			return false;

		SCR_BaseGameMode gamemode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		return gamemode.IsMaster();
	}

	//------------------------------------------------------------------------------------------------
	// Runs on a CallLater loop on the authority. For each objective, count live
	// units per faction inside the capture radius and resolve ownership.
	protected void EvaluateCaptures()
	{
		if (!IsServer())
			return;

		foreach (TG5_ObjectiveObject obj : m_aObjectives)
		{
			EvaluateObjective(obj);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EvaluateObjective(TG5_ObjectiveObject obj)
	{
		vector center = GetObjectivePos(obj);
		if (center == vector.Zero)
			return;

		m_aQueryResults.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, m_fCaptureRadius, QueryCharacter, null, EQueryEntitiesFlags.ALL);

		// Tally live characters per faction
		m_aFactionCounts.Clear();

		foreach (IEntity ent : m_aQueryResults)
		{
			FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
			if (!factionComp)
				continue;

			Faction faction = factionComp.GetAffiliatedFaction();
			if (!faction)
				continue;

			CountFactionPresence(faction);
		}

		Faction dominant = GetDominantFaction();
		if (!dominant || dominant.GetFactionKey() == obj.GetOwningFaction())
			return;

		// Authority applies the change locally, then broadcasts it
		FactionKey oldOwner = obj.GetOwningFaction();
		ApplyCapture(obj, dominant.GetFactionKey(), oldOwner);
		Rpc(RpcDo_CaptureBroadcast, m_aObjectives.Find(obj), dominant.GetFactionKey(), oldOwner);
	}

	//------------------------------------------------------------------------------------------------
	// Sphere query callback - only live characters count toward presence
	protected bool QueryCharacter(IEntity ent)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.GetLifeState() == ECharacterLifeState.ALIVE)
			m_aQueryResults.Insert(ent);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CountFactionPresence(Faction faction)
	{
		// Simple per-faction tally; index by faction manager index for stability
		FactionManager factionManager = GetGame().GetFactionManager();
		int factionIndex = factionManager.GetFactionIndex(faction);

		while (m_aFactionCounts.Count() <= factionIndex)
		{
			m_aFactionCounts.Insert(0);
		}

		int current = m_aFactionCounts[factionIndex];
		m_aFactionCounts[factionIndex] = current + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetDominantFaction()
	{
		FactionManager factionManager = GetGame().GetFactionManager();

		int bestIndex = -1;
		int bestCount = 0;

		for (int i = 0; i < m_aFactionCounts.Count(); i++)
		{
			if (m_aFactionCounts[i] > bestCount)
			{
				bestCount = m_aFactionCounts[i];
				bestIndex = i;
			}
		}

		if (bestIndex == -1)
			return null;

		return factionManager.GetFactionByIndex(bestIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetObjectivePos(TG5_ObjectiveObject obj)
	{
		if (obj.GetObjMapItem())
			return obj.GetObjMapItem().GetPos();

		if (obj.GetObjEntity())
			return obj.GetObjEntity().GetOrigin();

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	// Capture state change
	//------------------------------------------------------------------------------------------------

	protected void ApplyCapture(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner)
	{
		obj.SetOwningFaction(newOwner);
		m_OnObjectiveCaptured.Invoke(obj, newOwner, oldOwner);
	}

	//------------------------------------------------------------------------------------------------
	// Broadcast to every machine so client UI (map markers, notifications)
	// reacts identically. Vanilla convention: RPC methods are named
	// RpcDo_* / RpcAsk_* and carry primitive serializable args.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_CaptureBroadcast(int objectiveIndex, FactionKey newOwner, FactionKey oldOwner)
	{
		if (objectiveIndex < 0 || objectiveIndex >= m_aObjectives.Count())
			return;

		TG5_ObjectiveObject obj = m_aObjectives[objectiveIndex];
		if (!obj)
			return;

		// The authority already applied this in EvaluateObjective; on dedicated
		// servers with no proxies this RPC still returns to the authority, so
		// guard against double-application.
		if (obj.GetOwningFaction() == newOwner)
			return;

		ApplyCapture(obj, newOwner, oldOwner);
	}

	//------------------------------------------------------------------------------------------------
	// AI garrison spawning (authority only)
	//------------------------------------------------------------------------------------------------

	// Spawn a group at the objective and attach it as a defender.
	// Pattern per vanilla ambient patrol spawner: SpawnEntityPrefabEx ->
	// SCR_AIGroup.Cast -> SpawnUnits (if not immediate) -> AddWaypoint.
	array<SCR_AIGroup> SpawnGarrison(TG5_ObjectiveObject obj, ResourceName groupPrefab = "")
	{
		if (!IsServer())
			return null;

		if (groupPrefab.IsEmpty())
			groupPrefab = m_sDefaultGarrisonPrefab;

		if (groupPrefab.IsEmpty())
			return null;

		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = GetObjectivePos(obj);

		array<SCR_AIGroup> groups = new array<SCR_AIGroup>();
		for (int i = 0; i <= obj.GetInfGroupNum(); i++)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: params));
			
			if (!group)
				return null;

			if (!group.GetSpawnImmediately())
				group.SpawnUnits();
			
			groups.Insert(group);
		}

		obj.AddAiGroup(groups);

		// TODO: attach a defend waypoint at the objective position once the
		// waypoint prefab resource is configured, e.g. group.AddWaypoint(wp)

		return groups;
	}
}
