void TG5_OnObjectiveCapturedDelegate(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner);
typedef func TG5_OnObjectiveCapturedDelegate;
typedef ScriptInvokerBase<TG5_OnObjectiveCapturedDelegate> TG5_OnObjectiveCapturedInvoker;

void TG5_OnObjectiveContestedDelegate(TG5_ObjectiveObject obj, FactionKey contestingFaction);
typedef func TG5_OnObjectiveContestedDelegate;
typedef ScriptInvokerBase<TG5_OnObjectiveContestedDelegate> TG5_OnObjectiveContestedInvoker;

void TG5_OnObjectivesReadyDelegate();
typedef func TG5_OnObjectivesReadyDelegate;
typedef ScriptInvokerBase<TG5_OnObjectivesReadyDelegate> TG5_OnObjectivesReadyInvoker;

void TG5_OnCaptureProgressDelegate(TG5_ObjectiveObject obj, float progress, FactionKey capturingFaction);
typedef func TG5_OnCaptureProgressDelegate;
typedef ScriptInvokerBase<TG5_OnCaptureProgressDelegate> TG5_OnCaptureProgressInvoker;

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Callsign", description: "Authoritative manager for objectives: state, capture logic, and AI garrison spawning.")]
class TG5_ObjectiveManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
// Runs on server AND client, with a strict split:
//
//   authority  discovers objectives, spawns triggers, spawns garrisons,
//              evaluates captures, owns all objective state
//   client     receives the objective list through RplSave/RplLoad and
//              ownership changes through a broadcast RPC; draws markers
//
// Clients never scan the world and never spawn gameplay entities, so the two
// sides cannot disagree about which objectives exist or what order they are
// in - which is what makes the objective id safe to use as an RPC key.
class TG5_ObjectiveManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute(defvalue: "150", desc: "Radius (m) around an objective within which faction presence is evaluated.", category: "Objectives")]
	protected float m_fCaptureRadius;

	[Attribute(defvalue: "2", desc: "Seconds between capture evaluations on the authority.", category: "Objectives", params: "0.5 60 0.5")]
	protected float m_fCaptureCheckInterval;

	[Attribute(defvalue: "{D9130D20F5A6942F}Prefabs/Triggers/TG5_ObjectiveTriggerEntity.et", desc: "Presence trigger spawned at every objective (authority only).", category: "Objectives", params: "et")]
	protected ResourceName m_sObjectiveTriggerPrefab;

	[Attribute(defvalue: "{6F72F05752ED62A8}Prefabs/Groups/OPFOR/Group_USSR_FireGroup_Guard.et", desc: "Default AI group prefab used for objective garrisons (authority only).", category: "Objectives", params: "et")]
	protected ResourceName m_sDefaultGarrisonPrefab;

	[Attribute(defvalue: "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", desc: "Waypoint given to every garrison group so it holds its spawn position.", category: "Objectives", params: "et")]
	protected ResourceName m_sDefendWaypointPrefab;

	[Attribute(defvalue: "30", desc: "Radius (m) each garrison group defends around its own spawn position.", category: "Objectives", params: "5 200 1")]
	protected float m_fGarrisonDefendRadius;

	[Attribute(defvalue: "40", desc: "Minimum spacing (m) between garrison groups within one objective.", category: "Objectives", params: "0 200 1")]
	protected float m_fGarrisonGroupSpacing;

	[Attribute(defvalue: "10", desc: "Seconds required to capture an objective when uncontested.", category: "Objectives", params: "1 60 1")]
	protected float m_fCaptureTime;

	[Attribute(defvalue: "0.1", desc: "Capture progress increment per check interval.", category: "Objectives", params: "0.01 1.0 0.01")]
	protected float m_fCaptureProgressIncrement;

	// How many random spots to try before giving up on placing a group
	protected static const int GARRISON_PLACEMENT_ATTEMPTS = 12;

	// Local search radius (m) around a random spot for ground a group can stand on
	protected static const float GARRISON_PLACEMENT_SEARCH_RADIUS = 20;

	// Clearance (m) a group needs - roughly a fireteam's footprint
	protected static const float GARRISON_PLACEMENT_CLEARANCE = 2;

	//------------------------------------------------------------------------------------------------
	protected static TG5_ObjectiveManagerComponent s_Instance;

	protected ref array<ref TG5_ObjectiveObject> m_aObjectives = new array<ref TG5_ObjectiveObject>();

	protected ref TG5_OnObjectiveCapturedInvoker m_OnObjectiveCaptured = new TG5_OnObjectiveCapturedInvoker();
	protected ref TG5_OnObjectiveContestedInvoker m_OnObjectiveContested = new TG5_OnObjectiveContestedInvoker();
	protected ref TG5_OnObjectivesReadyInvoker m_OnObjectivesReady = new TG5_OnObjectivesReadyInvoker();
	protected ref TG5_OnCaptureProgressInvoker m_OnCaptureProgress = new TG5_OnCaptureProgressInvoker();

	// Marker rendering only. Never created on a dedicated server, which has no
	// map UI - the objective list itself lives here, not in the map component.
	protected ref TG5_MapObjectiveInit m_MapObjectives;

	// Scratch arrays reused by the capture evaluation
	protected ref array<IEntity> m_aQueryResults = new array<IEntity>();
	protected ref array<int> m_aFactionCounts = new array<int>();
	protected ref array<vector> m_aGarrisonSpots = new array<vector>();

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
	// Fires once the objective list is populated - immediately on the
	// authority, on arrival of the replicated list on a client.
	TG5_OnObjectivesReadyInvoker GetOnObjectivesReady()
	{
		return m_OnObjectivesReady;
	}

	//------------------------------------------------------------------------------------------------
	TG5_OnCaptureProgressInvoker GetOnCaptureProgress()
	{
		return m_OnCaptureProgress;
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
		super.OnWorldPostProcess(world);

		EnsureMapObjectives();

		// Both run on a listen server: IsConsoleApp is only true on a
		// dedicated server, and the host is also the authority.
		if (Replication.IsServer())
			InitAuthority();
	}

	//------------------------------------------------------------------------------------------------
	// Discover the world's objectives and stand up the authority-side gameplay
	// entities for them. Only ever called on the authority.
	protected void InitAuthority()
	{
		array<ref TG5_ObjectiveDef> defs = new array<ref TG5_ObjectiveDef>();

		TG5_ObjectiveScanner scanner = new TG5_ObjectiveScanner();
		scanner.Scan(defs);

		foreach (TG5_ObjectiveDef def : defs)
		{
			TG5_ObjectiveObject obj = CreateObjective(def);
			RegisterObjective(obj);

			obj.SetTrigger(SpawnObjectiveTrigger(obj));
			obj.BindTrigger();
		}

		m_OnObjectivesReady.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	// Single construction path for both the authority and the replicated
	// client list, so ids stay in step on every machine.
	protected TG5_ObjectiveObject CreateObjective(notnull TG5_ObjectiveDef def)
	{
		return new TG5_ObjectiveObject(this, m_aObjectives.Count(), def, m_fCaptureRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected TG5_ObjectiveTriggerEntity SpawnObjectiveTrigger(notnull TG5_ObjectiveObject obj)
	{
		if (m_sObjectiveTriggerPrefab.IsEmpty())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = obj.GetPos();

		return TG5_ObjectiveTriggerEntity.Cast(GetGame().SpawnEntityPrefabEx(m_sObjectiveTriggerPrefab, false, params: params));
	}

	//------------------------------------------------------------------------------------------------
	// The map component is UI-only and must not exist on a dedicated server:
	// SCR_MapEntity has no instance there.
	protected void EnsureMapObjectives()
	{
		if (m_MapObjectives || System.IsConsoleApp())
			return;

		m_MapObjectives = new TG5_MapObjectiveInit(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		// Capture evaluation is authority-only
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(EvaluateCaptures, m_fCaptureCheckInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~TG5_ObjectiveManagerComponent()
	{
		if (s_Instance == this)
			s_Instance = null;

		if (GetGame() && GetGame().GetCallqueue())
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
	// Replication
	//------------------------------------------------------------------------------------------------

	// Ships the authority's objective list to every client, including
	// join-in-progress. Clients rebuild the same objects in the same order,
	// which is what keeps the ids in RpcDo_CaptureBroadcast meaningful.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(m_aObjectives.Count());

		foreach (TG5_ObjectiveObject obj : m_aObjectives)
		{
			writer.WriteVector(obj.GetPos());
			writer.WriteString(obj.GetObjType());
			writer.WriteString(obj.GetDisplayName());
			writer.WriteInt(FactionKeyToIndex(obj.GetOwningFaction()));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		if (!reader.ReadInt(count))
			return false;

		m_aObjectives.Clear();

		for (int i = 0; i < count; i++)
		{
			vector pos;
			string objType, name;
			int factionIndex;

			if (!reader.ReadVector(pos) || !reader.ReadString(objType) || !reader.ReadString(name) || !reader.ReadInt(factionIndex))
				return false;

			TG5_ObjectiveDef def = new TG5_ObjectiveDef(pos, objType, name);

			TG5_ObjectiveObject obj = CreateObjective(def);
			obj.SetOwningFaction(IndexToFactionKey(factionIndex));
			RegisterObjective(obj);
		}

		// RplLoad can land before OnWorldPostProcess, so the marker component
		// may not exist yet
		EnsureMapObjectives();
		m_OnObjectivesReady.Invoke();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Faction indices are what goes over the wire: FactionKey is a string, and
	// the map descriptor colouring wants the index anyway.
	protected int FactionKeyToIndex(FactionKey key)
	{
		if (key.IsEmpty())
			return -1;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return -1;

		Faction faction = factionManager.GetFactionByKey(key);
		if (!faction)
			return -1;

		return factionManager.GetFactionIndex(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected FactionKey IndexToFactionKey(int index)
	{
		if (index < 0)
			return string.Empty;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return string.Empty;

		Faction faction = factionManager.GetFactionByIndex(index);
		if (!faction)
			return string.Empty;

		return faction.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	// Capture evaluation (authority only)
	//------------------------------------------------------------------------------------------------

	protected void EvaluateCaptures()
	{
		if (!Replication.IsServer())
			return;

		foreach (TG5_ObjectiveObject obj : m_aObjectives)
		{
			EvaluateObjective(obj);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Presence comes from the objective's own trigger: the engine already
	// tracks what is inside it on the trigger's update period, so there is no
	// reason to run a second sphere query over the whole world per objective.
	protected void EvaluateObjective(TG5_ObjectiveObject obj)
	{
		TG5_ObjectiveTriggerEntity trigger = obj.GetTrigger();
		if (!trigger)
			return;

		m_aQueryResults.Clear();
		trigger.GetEntitiesInside(m_aQueryResults);
		if (m_aQueryResults.IsEmpty())
		{
			// No one present - reset capture progress
			if (obj.IsBeingCaptured())
			{
				obj.ResetCaptureState();
				Rpc(RpcDo_CaptureProgressBroadcast, obj.GetId(), 0.0, -1, false);
			}
			return;
		}

		m_aFactionCounts.Clear();

		foreach (IEntity ent : m_aQueryResults)
		{
			// The trigger's last query can be a few seconds old
			if (!IsLiveCharacter(ent))
				continue;

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
		{
			// No dominant faction or owner is dominant - reset capture progress
			if (obj.IsBeingCaptured())
			{
				obj.ResetCaptureState();
				Rpc(RpcDo_CaptureProgressBroadcast, obj.GetId(), 0.0, -1, false);
			}
			return;
		}

		// Anyone else on the ground blocks the capture
		if (IsContested())
		{
			obj.SetContested(true);
			m_OnObjectiveContested.Invoke(obj, dominant.GetFactionKey());
			
			// Reset progress if contested
			if (obj.IsBeingCaptured())
			{
				obj.ResetCaptureState();
				Rpc(RpcDo_CaptureProgressBroadcast, obj.GetId(), 0.0, -1, false);
			}
			return;
		}

		// Start or continue capture progress
		obj.SetContested(false);
		obj.SetBeingCaptured(true);
		obj.SetCapturingFaction(dominant.GetFactionKey());

		// Increment capture progress
		float currentProgress = obj.GetCaptureProgress();
		float newProgress = currentProgress + m_fCaptureProgressIncrement;
		
		if (newProgress >= 1.0)
		{
			// Capture complete
			newProgress = 1.0;
			FactionKey oldOwner = obj.GetOwningFaction();
			ApplyCapture(obj, dominant.GetFactionKey(), oldOwner);
			obj.ResetCaptureState();
			Rpc(RpcDo_CaptureBroadcast, obj.GetId(), FactionKeyToIndex(dominant.GetFactionKey()), FactionKeyToIndex(oldOwner));
		}
		else
		{
			// Capture in progress
			obj.SetCaptureProgress(newProgress);
			Rpc(RpcDo_CaptureProgressBroadcast, obj.GetId(), newProgress, FactionKeyToIndex(dominant.GetFactionKey()), true);
			m_OnCaptureProgress.Invoke(obj, newProgress, dominant.GetFactionKey());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLiveCharacter(IEntity ent)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.ALIVE;
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
	protected bool IsContested()
	{
		int present = 0;

		for (int i = 0; i < m_aFactionCounts.Count(); i++)
		{
			if (m_aFactionCounts[i] > 0)
				present++;
		}

		return present > 1;
	}

	//------------------------------------------------------------------------------------------------
	// Capture state change
	//------------------------------------------------------------------------------------------------

	protected void ApplyCapture(TG5_ObjectiveObject obj, FactionKey newOwner, FactionKey oldOwner)
	{
		obj.SetOwningFaction(newOwner);
		obj.ResetCaptureState(); // Reset capture state when capture completes
		m_OnObjectiveCaptured.Invoke(obj, newOwner, oldOwner);
	}

	//------------------------------------------------------------------------------------------------
	protected TG5_ObjectiveObject FindObjectiveById(int id)
	{
		foreach (TG5_ObjectiveObject obj : m_aObjectives)
		{
			if (obj.GetId() == id)
				return obj;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Broadcast to every machine so client UI (map markers, notifications)
	// reacts identically. Vanilla convention: RPC methods are named
	// RpcDo_* / RpcAsk_* and carry primitive serializable args.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_CaptureBroadcast(int objectiveId, int newOwnerIndex, int oldOwnerIndex)
	{
		TG5_ObjectiveObject obj = FindObjectiveById(objectiveId);
		if (!obj)
			return;

		FactionKey newOwner = IndexToFactionKey(newOwnerIndex);

		// The authority already applied this in EvaluateObjective; on dedicated
		// servers with no proxies this RPC still returns to the authority, so
		// guard against double-application.
		if (obj.GetOwningFaction() == newOwner)
			return;

		ApplyCapture(obj, newOwner, IndexToFactionKey(oldOwnerIndex));
	}

	//------------------------------------------------------------------------------------------------
	// Broadcast capture progress updates to all clients for UI progress bar
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_CaptureProgressBroadcast(int objectiveId, float progress, int capturingFactionIndex, bool isBeingCaptured)
	{
		TG5_ObjectiveObject obj = FindObjectiveById(objectiveId);
		if (!obj)
			return;

		obj.SetCaptureProgress(progress);
		obj.SetCapturingFaction(IndexToFactionKey(capturingFactionIndex));
		obj.SetBeingCaptured(isBeingCaptured);
		
		if (!isBeingCaptured)
			obj.SetContested(false);

		m_OnCaptureProgress.Invoke(obj, progress, obj.GetCapturingFaction());
	}

	//------------------------------------------------------------------------------------------------
	// AI garrison spawning (authority only)
	//------------------------------------------------------------------------------------------------

	// Scatters one group per GetInfGroupNum across the objective's radius and
	// pins each of them to their own spot with a defend waypoint.
	array<SCR_AIGroup> SpawnGarrison(TG5_ObjectiveObject obj, ResourceName groupPrefab = "")
	{
		if (!Replication.IsServer() || !obj)
			return null;

		if (groupPrefab.IsEmpty())
			groupPrefab = m_sDefaultGarrisonPrefab;

		if (groupPrefab.IsEmpty())
			return null;

		vector center = obj.GetPos();
		float radius = obj.GetCaptureRadius();

		m_aGarrisonSpots.Clear();

		array<SCR_AIGroup> groups = new array<SCR_AIGroup>();

		for (int i = 0, count = obj.GetInfGroupNum(); i < count; i++)
		{
			vector spot;
			if (!FindGarrisonPosition(center, radius, spot))
				continue;

			m_aGarrisonSpots.Insert(spot);

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixIdentity4(params.Transform);
			params.Transform[3] = spot;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefabEx(groupPrefab, false, params: params));
			if (!group)
				continue;

			if (!group.GetSpawnImmediately())
				group.SpawnUnits();

			AddDefendWaypoint(group, spot);
			groups.Insert(group);
		}

		if (groups.IsEmpty())
			return null;

		obj.AddAiGroup(groups);
		return groups;
	}

	//------------------------------------------------------------------------------------------------
	// Uniform sampling over the disc - sqrt on the radius stops every group
	// clumping around the objective centre - then a local search for ground a
	// group can actually stand on. FindEmptyTerrainPosition traces the surface,
	// rejects water and refuses spots already occupied by geometry.
	protected bool FindGarrisonPosition(vector center, float radius, out vector outPos)
	{
		BaseWorld world = GetGame().GetWorld();

		for (int attempt = 0; attempt < GARRISON_PLACEMENT_ATTEMPTS; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = radius * Math.Sqrt(Math.RandomFloat01());

			vector candidate = center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);

			if (!SCR_WorldTools.FindEmptyTerrainPosition(outPos, candidate, GARRISON_PLACEMENT_SEARCH_RADIUS, GARRISON_PLACEMENT_CLEARANCE, GARRISON_PLACEMENT_CLEARANCE, TraceFlags.ENTS | TraceFlags.OCEAN, world))
				continue;

			if (IsTooCloseToOtherGroup(outPos))
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsTooCloseToOtherGroup(vector pos)
	{
		float minDistSq = m_fGarrisonGroupSpacing * m_fGarrisonGroupSpacing;

		foreach (vector spot : m_aGarrisonSpots)
		{
			if (vector.DistanceSqXZ(spot, pos) < minDistSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// A tight defend waypoint per group is how vanilla keeps AI holding a spot
	// rather than wandering the whole objective; fast init places them straight
	// onto their defensive positions instead of having them walk there.
	protected void AddDefendWaypoint(notnull SCR_AIGroup group, vector pos)
	{
		if (m_sDefendWaypointPrefab.IsEmpty())
			return;

		Resource res = Resource.Load(m_sDefendWaypointPrefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = pos;

		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params));
		if (!waypoint)
			return;

		waypoint.SetCompletionRadius(m_fGarrisonDefendRadius);

		SCR_DefendWaypoint defendWaypoint = SCR_DefendWaypoint.Cast(waypoint);
		if (defendWaypoint)
			defendWaypoint.SetFastInit(true);

		group.AddWaypoint(waypoint);
	}
}
