[BaseContainerProps(configRoot: true)]
class TG5_BuildableVehiclesConfig
{
    [Attribute(category: "Land Vehicles")]
	ref array<ref TG5_BuildableVehicleEntry> m_sBuildableLandVehicles;

    [Attribute(category: "Air Vehicles")]
	ref array<ref TG5_BuildableVehicleEntry> m_sBuildableAirVehicles;

    [Attribute(category: "Sea Vehicles")]
	ref array<ref TG5_BuildableVehicleEntry> m_sBuildableSeaVehicles;
}