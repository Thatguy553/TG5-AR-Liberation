enum ETG5_VehicleDomain
{
	LAND,
	SEA,
	AIR,
}

[BaseContainerProps()]
class TG5_BuildableVehicleEntry
{
	[Attribute(defvalue: "", desc: "Resource name")]
	ResourceName m_ResourceName;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(ETG5_VehicleDomain), desc: "Vehicle domain")]
	ETG5_VehicleDomain m_eDomain;

	[Attribute(defvalue: "0", desc: "Supplies cost")]
	int m_iSupplyCost;

	[Attribute(defvalue: "0", desc: "Ammo cost")]
	int m_iAmmoCost;

	[Attribute(defvalue: "0", desc: "Fuel cost")]
	int m_iFuelCost;
}