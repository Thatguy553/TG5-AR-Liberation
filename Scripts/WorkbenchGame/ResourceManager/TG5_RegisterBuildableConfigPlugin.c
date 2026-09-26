[WorkbenchPluginAttribute(
	name: "Add to Liberation Buildable Resource Config",
	wbModules: {"ResourceManager"},
	resourceTypes: {"et"}
)]
class TG5_RegisterBuildableConfigPlugin : ResourceManagerPlugin
{
	protected Resource m_ConfigResource;

	override void OnResourceContextMenu(notnull array<ResourceName> resources)
	{
		Print("========================================");
		Print("TG5 Resource Config Plugin");
		Print("========================================");

		foreach (ResourceName resource : resources)
		{
			Print("Selected resource: " + resource);
		}

		TestConfig();
	}

	protected void TestConfig()
	{
		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);

		if (!resourceManager)
		{
			Print("ERROR: Could not get ResourceManager module!");
			return;
		}

		// Change this to the ACTUAL addon prefix containing your Configs folder.
		string configPath = "{7AAC032B565E3D6B}Configs/TG5_BuildableVehiclesConfig.conf";

		Print("Looking for config at:");
		Print(configPath);

		MetaFile metaFile = resourceManager.GetMetaFile(configPath);

		if (!metaFile)
		{
			Print("ERROR: GetMetaFile returned NULL!");
			return;
		}

		Print("MetaFile found!");

		ResourceName configName =
		"{7AAC032B565E3D6B}Configs/TG5_BuildableVehiclesConfig.conf";

		Resource configResource = Resource.Load(configName);

		if (!m_ConfigResource)
		{
			Print("ERROR: Resource.Load returned NULL!");
			return;
		}

		if (!m_ConfigResource.IsValid())
		{
			Print("ERROR: Resource is invalid!");
			return;
		}

		Print("SUCCESS: Config loaded!");
	}
}