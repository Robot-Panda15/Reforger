//------------------------------------------------------------------------------------------------
//! Zoom / ADS for HMD UI: held gadget must have HMD_LaserDesignatorGadgetComponent first (plain binoculars never qualify).
//! Then: binocular gadgets use only IsZoomedView() (ADS can be active while merely holding). Optic-only designators use IsSightADSActive.
class HMD_HandheldOpticZoom
{
	//------------------------------------------------------------------------------------------------
	//! Held gadget root may not host HMD on the same entity (child mesh). Depth-first search.
	static HMD_LaserDesignatorGadgetComponent FindHmdDesignatorOnGadget(IEntity root)
	{
		if (!root)
			return null;
		HMD_LaserDesignatorGadgetComponent h = HMD_LaserDesignatorGadgetComponent.Cast(root.FindComponent(HMD_LaserDesignatorGadgetComponent));
		if (h)
			return h;
		IEntity child = root.GetChildren();
		while (child)
		{
			HMD_LaserDesignatorGadgetComponent f = FindHmdDesignatorOnGadget(child);
			if (f)
				return f;
			child = child.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_2DOpticsComponent Find2DOpticsInHierarchy(IEntity root)
	{
		if (!root)
			return null;
		SCR_2DOpticsComponent o = SCR_2DOpticsComponent.Cast(root.FindComponent(SCR_2DOpticsComponent));
		if (o)
			return o;
		IEntity child = root.GetChildren();
		while (child)
		{
			SCR_2DOpticsComponent f = Find2DOpticsInHierarchy(child);
			if (f)
				return f;
			child = child.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_BinocularsComponent FindBinocularsInHierarchy(IEntity root)
	{
		if (!root)
			return null;
		SCR_BinocularsComponent b = SCR_BinocularsComponent.Cast(root.FindComponent(SCR_BinocularsComponent));
		if (b)
			return b;
		IEntity child = root.GetChildren();
		while (child)
		{
			SCR_BinocularsComponent f = FindBinocularsInHierarchy(child);
			if (f)
				return f;
			child = child.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseSightsComponent FindBaseSightsInHierarchy(IEntity root)
	{
		if (!root)
			return null;
		BaseSightsComponent s = BaseSightsComponent.Cast(root.FindComponent(BaseSightsComponent));
		if (s)
			return s;
		IEntity child = root.GetChildren();
		while (child)
		{
			BaseSightsComponent f = FindBaseSightsInHierarchy(child);
			if (f)
				return f;
			child = child.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity ResolveLocalCharacterEntity()
	{
		IEntity main = SCR_PlayerController.GetLocalMainEntity();
		if (main)
			return main;
		return SCR_PlayerController.GetLocalControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! True when looking through handheld optics relevant to HMD (designator gadget + zoom, or ADS on optic-only gadgets).
	static bool IsZoomedForHMD()
	{
		IEntity localChar = ResolveLocalCharacterEntity();
		if (!localChar)
			return false;

		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(localChar);
		if (!gm)
			return false;

		IEntity held = gm.GetHeldGadget();
		if (!held)
			return false;

		if (!FindHmdDesignatorOnGadget(held))
			return false;

		//! Binocular + designator: IsSightADSActive is often true while holding before raising; only real zoom counts.
		if (FindBinocularsInHierarchy(held))
			return SCR_BinocularsComponent.IsZoomedView();

		if (SCR_BinocularsComponent.IsZoomedView())
			return true;

		SCR_2DOpticsComponent optics = Find2DOpticsInHierarchy(held);
		if (optics && optics.IsSightADSActive())
			return true;

		BaseSightsComponent sights = FindBaseSightsInHierarchy(held);
		if (sights && sights.IsSightADSActive())
			return true;

		return false;
	}
}
