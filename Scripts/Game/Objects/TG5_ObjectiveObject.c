// Data container describing a single map objective.
// Owns its runtime marker widget while the map is open (null when the map is closed).
class TG5_ObjectiveObject
{
	protected string m_sObjType;
	protected IEntity m_eObjEntity;
	protected Widget m_wObjWidget;
	protected MapItem m_ObjMapItem;

	// Current controlling faction, set authoritatively by the objective
	// manager and replicated to clients via the manager's broadcast RPC
	protected FactionKey m_OwningFaction;

	protected ref array<AIGroup> m_aAiObjUnits = new array<AIGroup>();

	string GetObjType() { return m_sObjType; }
	IEntity GetObjEntity() { return m_eObjEntity; }
	Widget GetObjWidget() { return m_wObjWidget; }
	MapItem GetObjMapItem() { return m_ObjMapItem; }
	FactionKey GetOwningFaction() { return m_OwningFaction; }

	void SetObjType(string type) { m_sObjType = type; }
	void SetObjEntity(IEntity ent) { m_eObjEntity = ent; }
	void SetObjWidget(Widget w) { m_wObjWidget = w; }
	void SetObjMapItem(MapItem mapItem) { m_ObjMapItem = mapItem; }
	void SetOwningFaction(FactionKey faction) { m_OwningFaction = faction; }

	int AddAiGroup(AIGroup group) { return m_aAiObjUnits.Insert(group); }
	bool RemoveAiGroup(AIGroup group) { return m_aAiObjUnits.RemoveItem(group); }
	void RemoveAiGroup(int index) { m_aAiObjUnits.Remove(index); }
	int GetAiGroupCount() { return m_aAiObjUnits.Count(); }
}
