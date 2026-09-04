[EntityEditorProps(category: "GameScripted/Callsign", description: "Runtime player-presence trigger for objectives")]
class TG5_ObjectiveTriggerEntityClass : SCR_BaseTriggerEntityClass {}

class TG5_ObjectiveTriggerEntity : SCR_BaseTriggerEntity
{
	// Only live player-controlled characters activate this trigger
	override bool ScriptedEntityFilterForQuery(IEntity ent)
	{
		if (!IsAlive(ent))
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		
		if (!character)
			return false;

		// Player only - ignore AI. Swap this check if AI should also trigger it.
		return EntityUtils.IsPlayer(ent);
	}
	
	override ScriptInvoker GetOnActivate()
	{
		Print("[GetOnActivate] Activating");
		super.GetOnActivate();
		return m_OnActivate;
	}
}