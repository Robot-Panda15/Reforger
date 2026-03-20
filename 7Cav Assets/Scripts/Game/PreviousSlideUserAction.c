//------------------------------------------------------------------------------------------------
// PreviousSlideUserAction - Goes back to the previous slide prefab. Hidden when only 1 slide.
// Add to prefab's ActionsManagerComponent. Calls CycleToPreviousSlide on DecalMaterialSwitcherComponent.
// Supports ActionsManagerComponent on parent OR on decal child (for entities where raycast hits child).
//------------------------------------------------------------------------------------------------

class PreviousSlideUserAction: ScriptedUserAction
{
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
	}

	DecalMaterialSwitcherComponent FindSwitcher(IEntity entity)
	{
		if (!entity)
			return null;
		DecalMaterialSwitcherComponent switcher = DecalMaterialSwitcherComponent.Cast(entity.FindComponent(DecalMaterialSwitcherComponent));
		if (switcher)
			return switcher;
		IEntity parent = entity.GetParent();
		if (parent)
			return DecalMaterialSwitcherComponent.Cast(parent.FindComponent(DecalMaterialSwitcherComponent));
		return null;
	}

	override bool CanBeShownScript(IEntity user)
	{
		DecalMaterialSwitcherComponent switcher = FindSwitcher(GetOwner());
		if (!switcher)
			return false;
		int count = switcher.GetSlideCount();
		int current = switcher.GetCurrentIndex();
		return count > 1 && current > 0;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		DecalMaterialSwitcherComponent switcher = FindSwitcher(pOwnerEntity);
		if (switcher)
			switcher.CycleToPreviousSlide();
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Previous Slide";
		return true;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	override bool CanBroadcastScript()
	{
		return true;
	}
}
