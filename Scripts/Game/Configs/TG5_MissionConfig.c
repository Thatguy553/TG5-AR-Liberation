[BaseContainerProps(configRoot: true)]
class TG5_MissionConfig
{
    // Objective Manager Settings
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

	[Attribute(defvalue: "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et", desc: "Vehicle prefab spawned for heavy vehicle slots in an objective garrison.", category: "Objectives", params: "et")]
	protected ResourceName m_sHeavyVehiclePrefab;

	[Attribute(defvalue: "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et", desc: "Vehicle prefab spawned for light vehicle slots in an objective garrison.", category: "Objectives", params: "et")]
	protected ResourceName m_sLightVehiclePrefab;

	[Attribute(defvalue: "30", desc: "Radius (m) each garrison group defends around its own spawn position.", category: "Objectives", params: "5 200 1")]
	protected float m_fGarrisonDefendRadius;

	[Attribute(defvalue: "40", desc: "Minimum spacing (m) between garrison groups within one objective.", category: "Objectives", params: "0 200 1")]
	protected float m_fGarrisonGroupSpacing;

	[Attribute(defvalue: "30", desc: "Seconds after the last player leaves before the objective garrison is removed.", category: "Objectives", params: "0 600 1")]
	protected float m_fGarrisonDespawnDelay;

	[Attribute(defvalue: "10", desc: "Seconds required to capture an objective when uncontested.", category: "Objectives", params: "1 60 1")]
	protected float m_fCaptureTime;

	[Attribute(defvalue: "0.1", desc: "Capture progress increment per check interval.", category: "Objectives", params: "0.01 1.0 0.01")]
	protected float m_fCaptureProgressIncrement;

	// Objective Defender Settings
	// City
	[Attribute(desc: "City Config", category: "Objective Configs")]
	protected TG5_ObjectiveDataEntry m_cCityConfig;

	// Military
	[Attribute(desc: "Military Config", category: "Objective Configs")]
	protected TG5_ObjectiveDataEntry m_cMilitaryConfig;

	// Factory
	[Attribute(desc: "Factory Config", category: "Objective Configs")]
	protected TG5_ObjectiveDataEntry m_cFactoryConfig;

	// Town
	[Attribute(desc: "Town Config", category: "Objective Configs")]
	protected TG5_ObjectiveDataEntry m_cTownConfig;

	// Radio
	[Attribute(desc: "Radio Config", category: "Objective Configs")]
	protected TG5_ObjectiveDataEntry m_cRadioConfig;

	//------------------------------------------------------------------------------------------------
	float GetCaptureRadius()
	{ return m_fCaptureRadius; }

	float GetCaptureCheckInterval()
	{ return m_fCaptureCheckInterval; }

	ResourceName GetObjectiveTriggerPrefab()
	{ return m_sObjectiveTriggerPrefab; }

	ResourceName GetDefaultGarrisonPrefab()
	{ return m_sDefaultGarrisonPrefab; }

	ResourceName GetDefendWaypointPrefab()
	{ return m_sDefendWaypointPrefab; }

	ResourceName GetHeavyVehiclePrefab()
	{ return m_sHeavyVehiclePrefab; }

	ResourceName GetLightVehiclePrefab()
	{ return m_sLightVehiclePrefab; }

	float GetGarrisonDefendRadius()
	{ return m_fGarrisonDefendRadius; }

	float GetGarrisonGroupSpacing()
	{ return m_fGarrisonGroupSpacing; }

	float GetGarrisonDespawnDelay()
	{ return m_fGarrisonDespawnDelay; }

	float GetCaptureTime()
	{ return m_fCaptureTime; }

	float GetCaptureProgressIncrement()
	{ return m_fCaptureProgressIncrement; }

	TG5_ObjectiveDataEntry GetCityConfig()
	{ return m_cCityConfig; }

	TG5_ObjectiveDataEntry GetMilitaryConfig()
	{ return m_cMilitaryConfig; }

	TG5_ObjectiveDataEntry GetFactoryConfig()
	{ return m_cFactoryConfig; }

	TG5_ObjectiveDataEntry GetTownConfig()
	{ return m_cTownConfig; }

	TG5_ObjectiveDataEntry GetRadioConfig()
	{ return m_cRadioConfig; }
}