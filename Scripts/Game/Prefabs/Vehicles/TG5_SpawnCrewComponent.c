class TG5_SpawnCrewComponentClass: ScriptComponentClass {}
class TG5_SpawnCrewComponent: ScriptComponent {
    override void OnPostInit(IEntity owner) { SetEventMask(owner, EntityEvent.INIT); }
    
    override void EOnInit(IEntity owner) { 
        if (SCR_Global.IsEditMode() || !Replication.IsServer())
            return;
    }
    
    // Thx Bacon!!
    void SpawnOccupants() {
        SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(GetOwner().FindComponent(SCR_BaseCompartmentManagerComponent));
        compartmentManager.SpawnDefaultOccupants({ECompartmentType.TURRET, ECompartmentType.PILOT});
    }
}