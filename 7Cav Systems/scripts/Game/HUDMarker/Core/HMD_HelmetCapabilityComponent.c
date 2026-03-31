//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Marks this helmet prefab as HMD-capable for HUDMarkerSystem (full vehicle HUD when not using prefab list, or in addition to list matching).")]
class HMD_HelmetCapabilityComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Tag component: add to helmet prefabs that grant full HMD HUD in vehicles (see HUDMarkerSystem helmet config).
class HMD_HelmetCapabilityComponent : ScriptComponent
{
}
