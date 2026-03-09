class ParachuteItemComponentExtendedClass : ParachuteItemComponentClass {}
class ParachuteItemComponentExtended : ParachuteItemComponent
{
	[Attribute("{B1C2D3E4F5A6B7C8}Prefabs/DeployedParachutes/Core/DeployedParachuteMK4Extended.et", UIWidgets.ResourceNamePicker, "Parachute Prefab (Extended)", "et")]
	protected ResourceName m_ExtendedParachutePrefab;

	override ResourceName GetParachutePrefab()
	{
		if (m_ExtendedParachutePrefab != "")
			return m_ExtendedParachutePrefab;
		return super.GetParachutePrefab();
	}
}
