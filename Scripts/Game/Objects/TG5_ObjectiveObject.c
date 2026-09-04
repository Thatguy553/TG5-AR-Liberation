// Data container describing a single map objective.
// Owns its runtime marker widget while the map is open (null when the map is closed).
class TG5_ObjectiveObject
{
	protected string m_sObjType;
	protected IEntity m_eObjEntity;
	protected Widget m_wObjWidget;
	protected MapItem m_mObjMapItem;
	protected TG5_ObjectiveTriggerEntity m_tTrigger;
	
	protected int m_iInfGroupNum = 0;
	protected int m_iVehNum = 0;
	
	protected bool m_bActive = false;
	
	protected TG5_ObjectiveManagerComponent m_cObjMngr;

	// Current controlling faction, set authoritatively by the objective
	// manager and replicated to clients via the manager's broadcast RPC
	protected FactionKey m_OwningFaction;

	protected ref array<AIGroup> m_aAiObjUnits = new array<AIGroup>();

	string GetObjType() { return m_sObjType; }
	IEntity GetObjEntity() { return m_eObjEntity; }
	Widget GetObjWidget() { return m_wObjWidget; }
	MapItem GetObjMapItem() { return m_mObjMapItem; }
	FactionKey GetOwningFaction() { return m_OwningFaction; }
	int GetInfGroupNum() { return m_iInfGroupNum; }
	int GetVehNum() { return m_iVehNum; }
	TG5_ObjectiveTriggerEntity GetTrigger() { return m_tTrigger; };

	void SetObjType(string type) { 
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
	
	
	void SetObjEntity(IEntity ent) { m_eObjEntity = ent; }
	void SetObjWidget(Widget w) { m_wObjWidget = w; }
	void SetObjMapItem(MapItem mapItem) { m_mObjMapItem = mapItem; }
	void SetOwningFaction(FactionKey faction) { m_OwningFaction = faction; }
	void SetTrigger(TG5_ObjectiveTriggerEntity trigger) { m_tTrigger = trigger; };

	int AddAiGroup(SCR_AIGroup group) { return m_aAiObjUnits.Insert(group); }
	void AddAiGroup(array<SCR_AIGroup> groups) { 
		foreach (SCR_AIGroup group : groups) {
			m_aAiObjUnits.Insert(group);
		}
	}
	bool RemoveAiGroup(AIGroup group) { return m_aAiObjUnits.RemoveItem(group); }
	void RemoveAiGroup(int index) { m_aAiObjUnits.Remove(index); }
	int GetAiGroupCount() { return m_aAiObjUnits.Count(); }
	
	void Activate()
	{
		if (!m_bActive)
		{
			Print(string.Format("[Sector Activating] %1", m_mObjMapItem.GetDisplayName()));
			m_cObjMngr.SpawnGarrison(this);
			m_bActive = true;
		}
	}
	
	void DeActivate()
	{
		m_bActive = false;
	}
	
	// TG5_ObjectiveObject
	void BindTrigger()
	{
		if (!m_tTrigger)
			return;
	
		m_tTrigger.GetOnActivate().Insert(Activate);
		m_tTrigger.GetOnDeactivate().Insert(DeActivate);
	}
	
	void TG5_ObjectiveObject()
	{
		m_cObjMngr = TG5_ObjectiveManagerComponent.GetInstance();
	}
}
