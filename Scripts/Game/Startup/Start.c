[ComponentEditorProps(category: "GameScripted/Callsign", description: "")]
class TG5_StartManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

// Startup/bootstrap concerns only. Objective ownership, capture logic, AI
// spawning, and the map marker component all live on TG5_ObjectiveManagerComponent.
class TG5_StartManagerComponent : SCR_BaseGameModeComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnWorldPostProcess(World world)
	{
		// TODO: world bootstrap that isn't objective management
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
	}
}
