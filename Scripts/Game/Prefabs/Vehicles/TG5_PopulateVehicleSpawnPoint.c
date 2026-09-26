class TG5_PopulateVehicleSpawnPointComponentClass: ScriptComponentClass {}
class TG5_PopulateVehicleSpawnPointComponent: ScriptComponent {
    [Attribute(defvalue: "150", desc: "Radius (m) around an objective within which faction presence is evaluated.", category: "Objectives")]
	protected float m_fCaptureRadius;

    [Attribute(defvalue: "0", UIWidgets.ComboBox, "Vehicle to spawn")]
	protected string m_sVehicleToSpawn;

    override void OnPostInit(IEntity owner) { SetEventMask(owner, EntityEvent.INIT); }
    
    override void EOnInit(IEntity owner) { 
        if (SCR_Global.IsEditMode() || !Replication.IsServer())
            return;
    }
    
    
}

