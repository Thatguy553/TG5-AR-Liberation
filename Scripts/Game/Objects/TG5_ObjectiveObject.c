// Data container describing a single map objective.
// Position, name and type are copied from the descriptor at construction: the
// engine owns MapItem instances (they can be recycled) and they don't exist at
// all on a dedicated server, so nothing here may hold one.
// Owns its runtime marker widget while the map is open (null when the map is closed).
class TG5_ObjectiveObject
{
	// Stable across every machine - assigned by the authority in list order and
	// used as the identifier in capture RPCs
	protected int m_iId = -1;

	protected string m_sObjType;
	protected string m_sName;
	protected vector m_vPos;
	protected float m_fCaptureRadius;

	protected Widget m_wObjWidget;
	protected TG5_ObjectiveTriggerEntity m_tTrigger;

	protected int m_iInfGroupNum = 0;
	protected int m_iVehNum = 0;

	protected bool m_bActive = false;

	protected TG5_ObjectiveManagerComponent m_cObjMngr;

	// Current controlling faction, set authoritatively by the objective
	// manager and replicated to clients via the manager's broadcast RPC
	protected FactionKey m_OwningFaction;

	protected ref array<SCR_AIGroup> m_aAiObjUnits = new array<SCR_AIGroup>();

	int GetId() { return m_iId; }
	string GetObjType() { return m_sObjType; }
	string GetDisplayName() { return m_sName; }
	vector GetPos() { return m_vPos; }
	float GetCaptureRadius() { return m_fCaptureRadius; }
	Widget GetObjWidget() { return m_wObjWidget; }
	FactionKey GetOwningFaction() { return m_OwningFaction; }
	int GetInfGroupNum() { return m_iInfGroupNum; }
	int GetVehNum() { return m_iVehNum; }
	bool IsActive() { return m_bActive; }
	TG5_ObjectiveTriggerEntity GetTrigger() { return m_tTrigger; }

	//------------------------------------------------------------------------------------------------
	void SetObjType(string type)
	{
		m_sObjType = type;

		switch (type)
		{
			case "city":
				m_iInfGroupNum = 10;
				m_iVehNum = 4;
				return;
			case "military":
				m_iInfGroupNum = 5;
				m_iVehNum = 3;
				return;
			case "factory":
				m_iInfGroupNum = 3;
				m_iVehNum = 1;
				return;
			case "town":
				m_iInfGroupNum = 4;
				m_iVehNum = 1;
				return;
			case "radio":
				m_iInfGroupNum = 2;
				m_iVehNum = 1;
				return;
		}
	}

	void SetObjWidget(Widget w) { m_wObjWidget = w; }
	void SetOwningFaction(FactionKey faction) { m_OwningFaction = faction; }
	void SetTrigger(TG5_ObjectiveTriggerEntity trigger) { m_tTrigger = trigger; }

	//------------------------------------------------------------------------------------------------
	int AddAiGroup(SCR_AIGroup group) { return m_aAiObjUnits.Insert(group); }

	void AddAiGroup(array<SCR_AIGroup> groups)
	{
		if (!groups)
			return;

		foreach (SCR_AIGroup group : groups)
		{
			m_aAiObjUnits.Insert(group);
		}
	}

	bool RemoveAiGroup(SCR_AIGroup group) { return m_aAiObjUnits.RemoveItem(group); }
	void RemoveAiGroup(int index) { m_aAiObjUnits.Remove(index); }
	int GetAiGroupCount() { return m_aAiObjUnits.Count(); }

	//------------------------------------------------------------------------------------------------
	// Fired by the trigger when the first player enters. Authority only in
	// practice: clients never spawn a trigger, and SpawnGarrison is guarded.
	void Activate()
	{
		if (m_bActive || !m_cObjMngr)
			return;

		// Set before spawning so a re-entrant activation can't double-garrison
		m_bActive = true;
		m_cObjMngr.SpawnGarrison(this);
	}

	//------------------------------------------------------------------------------------------------
	void DeActivate()
	{
		m_bActive = false;
	}

	//------------------------------------------------------------------------------------------------
	void BindTrigger()
	{
		if (!m_tTrigger)
			return;

		m_tTrigger.GetOnActivate().Insert(Activate);
		m_tTrigger.GetOnDeactivate().Insert(DeActivate);
		m_tTrigger.SetSphereRadius(m_fCaptureRadius);
	}

	//------------------------------------------------------------------------------------------------
	// The manager is passed in rather than pulled from a static: an objective
	// is useless without one, and the explicit dependency makes it impossible
	// to construct against a manager that isn't the one that owns this list.
	void TG5_ObjectiveObject(notnull TG5_ObjectiveManagerComponent manager, int id, notnull TG5_ObjectiveDef def, float captureRadius)
	{
		m_cObjMngr = manager;
		m_iId = id;
		m_vPos = def.m_vPos;
		m_sName = def.m_sName;
		m_fCaptureRadius = captureRadius;

		SetObjType(def.m_sType);
	}
}
