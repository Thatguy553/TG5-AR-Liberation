class SCR_MapMarkerBaseMilitaryClass : SCR_MapMarkerEntityClass
{
}

class SCR_MapMarkerBaseMilitary : SCR_MapMarkerEntity
{	
	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		SetGlobalVisible(true);
		m_eType = SCR_EMapMarkerType.DYNAMIC_EXAMPLE;
		m_Target = this;
		
		SetImage("{7CD99D22C7AE8195}UI/Textures/GroupManagement/FlagIcons/GroupFlagsOpfor.imageset", "artillery");
	}
	
}