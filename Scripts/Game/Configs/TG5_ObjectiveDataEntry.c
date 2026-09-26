enum ETG5_ObjectiveTypes
{
	CITY,
	TOWN,
	MILITARY,
    FACTORY,
    RADIO
}

[BaseContainerProps()]
class TG5_ObjectiveDataEntry
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(ETG5_ObjectiveTypes), desc: "Base Type")]
	ETG5_ObjectiveTypes m_eType;

    [Attribute(defvalue: "0", desc: "Min Spawnable Infantry Groups")]
	int m_iMinInfantryGroups;

	[Attribute(defvalue: "10", desc: "Max Spawnable Infantry Groups")]
	int m_iMaxInfantryGroups;

    [Attribute(defvalue: "0", desc: "Min Spawnable Heavy Vehicles")]
	int m_iMinHeavyVehicles;

	[Attribute(defvalue: "3", desc: "Max Spawnable Heavy Vehicles")]
	int m_iMaxHeavyVehicles;

    [Attribute(defvalue: "0", desc: "Min Spawnable Light Vehicles")]
	int m_iMinLightVehicles;

	[Attribute(defvalue: "3", desc: "Max Spawnable Light Vehicles")]
	int m_iMaxLightVehicles;
}