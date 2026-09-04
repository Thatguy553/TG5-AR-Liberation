[EntityEditorProps(category: "GameScripted/Callsign", description: "Runtime presence trigger for objectives")]
class TG5_ObjectiveTriggerEntityClass : SCR_BaseTriggerEntityClass {}

//------------------------------------------------------------------------------------------------
// Tracks every live character inside an objective, not just players: the
// objective manager reads GetEntitiesInside() to tally faction presence, so
// the trigger's periodic query doubles as the capture check and no second
// sphere query over the world is needed.
//
// Activation still means "a player showed up" - the garrison must not spawn
// because its own AI wandered into the sphere.
class TG5_ObjectiveTriggerEntity : SCR_BaseTriggerEntity
{
	override bool ScriptedEntityFilterForQuery(IEntity ent)
	{
		if (!IsAlive(ent))
			return false;

		return SCR_ChimeraCharacter.Cast(ent) != null;
	}

	//------------------------------------------------------------------------------------------------
	override protected event void OnActivate(IEntity ent)
	{
		if (!EntityUtils.IsPlayer(ent))
			return;

		super.OnActivate(ent);
	}

	//------------------------------------------------------------------------------------------------
	// Deactivation is per-entity, so hold it back until the last player has
	// left - otherwise one of two players leaving deactivates the objective
	// and the next one to walk in re-triggers activation.
	override protected event void OnDeactivate(IEntity ent)
	{
		if (!EntityUtils.IsPlayer(ent) || HasPlayerInside(ent))
			return;

		super.OnDeactivate(ent);
	}

	//------------------------------------------------------------------------------------------------
	// ignoreEnt is the entity currently leaving, which may still be listed in
	// the results of the last periodic query.
	bool HasPlayerInside(IEntity ignoreEnt = null)
	{
		array<IEntity> inside = new array<IEntity>();
		GetEntitiesInside(inside);

		foreach (IEntity ent : inside)
		{
			if (ent == ignoreEnt)
				continue;

			if (EntityUtils.IsPlayer(ent) && IsAlive(ent))
				return true;
		}

		return false;
	}
}
