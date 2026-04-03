//------------------------------------------------------------------------------------------------
//! Pickup blocking uses dismantle visibility: when SCR_DismantleInventoryItemBaseAction would show on the same
//! entity (ActionsManagerComponent action list), SCR_PickUpItemAction is hidden/blocked. Optional m_bDeployed OR-blocks for non-vanilla flows.
[ComponentEditorProps(category: "Inventory", description: "Optional extra block when m_bDeployed (server). Primary block is dismantle action visibility.")]
class BlockDeployPickupComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class BlockDeployPickupComponent : ScriptComponent
{
	[RplProp(onRplName: "OnDeployedReplicated")]
	protected bool m_bDeployed;

	//------------------------------------------------------------------------------------------------
	void OnDeployedReplicated()
	{
	}

	//------------------------------------------------------------------------------------------------
	bool IsDeployed()
	{
		return m_bDeployed;
	}

	//------------------------------------------------------------------------------------------------
	void SetDeployed(bool deployed)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			m_bDeployed = deployed;
			if (Replication.IsRunning())
				Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	static BlockDeployPickupComponent FindOnEntity(IEntity owner)
	{
		if (!owner)
			return null;
		return BlockDeployPickupComponent.Cast(owner.FindComponent(BlockDeployPickupComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! True when vanilla dismantle action would show for this user (deployed state).
	static bool IsDismantleInventoryItemActionShown(IEntity itemEntity, IEntity user)
	{
		if (!itemEntity || !user)
			return false;
		ActionsManagerComponent am = ActionsManagerComponent.Cast(itemEntity.FindComponent(ActionsManagerComponent));
		if (!am)
			return false;
		array<BaseUserAction> actions = {};
		am.GetActionsList(actions);
		for (int i = 0; i < actions.Count(); i++)
		{
			BaseUserAction ba = actions[i];
			if (!ba)
				continue;
			SCR_DismantleInventoryItemBaseAction dismantle = SCR_DismantleInventoryItemBaseAction.Cast(ba);
			if (!dismantle)
				continue;
			if (dismantle.CanBeShownScript(user))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool ShouldBlockPickupForItem(IEntity itemEntity, IEntity user)
	{
		if (!itemEntity || !user)
			return false;
		if (IsDismantleInventoryItemActionShown(itemEntity, user))
			return true;
		BlockDeployPickupComponent c = FindOnEntity(itemEntity);
		return c && c.IsDeployed();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity ResolveItemEntityFromInventoryAction(SCR_InventoryAction invAction)
	{
		if (!invAction)
			return null;
		InventoryItemComponent itemComp = invAction.m_Item;
		if (itemComp)
		{
			IEntity itemEnt = itemComp.GetOwner();
			if (itemEnt)
				return itemEnt;
		}
		return invAction.GetOwner();
	}
}

//------------------------------------------------------------------------------------------------
#ifndef DISABLE_INVENTORY
modded class SCR_PickUpItemAction : SCR_InventoryAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BlockDeployPickupComponent.ShouldBlockPickupForItem(BlockDeployPickupComponent.ResolveItemEntityFromInventoryAction(this), user))
			return false;
		return super.CanBeShownScript(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (BlockDeployPickupComponent.ShouldBlockPickupForItem(BlockDeployPickupComponent.ResolveItemEntityFromInventoryAction(this), user))
			return false;
		if (!super.CanBePerformedScript(user))
			return false;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_DeployableInventoryItemPickUpAction : SCR_PickUpItemAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BlockDeployPickupComponent.ShouldBlockPickupForItem(BlockDeployPickupComponent.ResolveItemEntityFromInventoryAction(this), user))
			return false;
		return super.CanBeShownScript(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (BlockDeployPickupComponent.ShouldBlockPickupForItem(BlockDeployPickupComponent.ResolveItemEntityFromInventoryAction(this), user))
			return false;
		return super.CanBePerformedScript(user);
	}
}
#endif