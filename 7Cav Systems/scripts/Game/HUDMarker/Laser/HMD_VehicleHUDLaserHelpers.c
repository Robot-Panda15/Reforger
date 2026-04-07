//------------------------------------------------------------------------------------------------
//! Compartment name helpers for HUD laser visibility / marking (match Workbench compartment Unique name).
class HMD_VehicleHUDLaserHelpers
{
	//------------------------------------------------------------------------------------------------
	//! Compartment owner may be a child (e.g. turret). Walk up until HMD_HudMarkerEligibilityVehicleComponent (hull) is found.
	//! Turret marking: HUDLaserTurretMarkingComponent on the turret; camera marking: HUDLaserCameraMarkingComponent on hull/seat entity.
	static IEntity ResolveVehicleHudMarkerEligibilityVehicleRoot(IEntity slotOwner)
	{
		IEntity ent = slotOwner;
		while (ent)
		{
			if (ent.FindComponent(HMD_HudMarkerEligibilityVehicleComponent))
				return ent;
			ent = ent.GetParent();
		}
		return slotOwner;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity ResolveVehicleHUDMarkingRoot(IEntity slotOwner)
	{
		IEntity ent = slotOwner;
		while (ent)
		{
			if (ent.FindComponent(HUDLaserCameraMarkingComponent) || ent.FindComponent(HUDLaserTurretMarkingComponent))
				return ent;
			ent = ent.GetParent();
		}
		return slotOwner;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves camera vs turret marking implementation on an entity.
	static HUDLaserMarkingComponent FindMarkingComponentOnEntity(IEntity ent)
	{
		if (!ent)
			return null;
		HUDLaserMarkingComponent m = HUDLaserMarkingComponent.Cast(ent.FindComponent(HUDLaserTurretMarkingComponent));
		if (!m)
			m = HUDLaserMarkingComponent.Cast(ent.FindComponent(HUDLaserCameraMarkingComponent));
		return m;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefer GetCompartmentUniqueName(); if empty, GetCompartmentName(true).
	static string GetSlotIdentityName(BaseCompartmentSlot slot)
	{
		if (!slot)
			return "";
		string un = slot.GetCompartmentUniqueName();
		if (un && un != "")
			return un;
		string dn = slot.GetCompartmentName(true);
		if (dn && dn != "")
			return dn;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	static string TrimWhitespace(string s)
	{
		if (!s || s == "")
			return "";
		int start = 0;
		int end = s.Length() - 1;
		while (start <= end)
		{
			string ch = s.Substring(start, 1);
			if (ch != " " && ch != "\t")
				break;
			start++;
		}
		while (end >= start)
		{
			string ch2 = s.Substring(end, 1);
			if (ch2 != " " && ch2 != "\t")
				break;
			end--;
		}
		if (start > end)
			return "";
		return s.Substring(start, end - start + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Fills outNames from comma-separated list (spaces around commas trimmed per token).
	static void SplitCommaSeparatedNames(string csv, notnull array<string> outNames)
	{
		outNames.Clear();
		if (!csv || csv == "")
			return;
		int len = csv.Length();
		int start = 0;
		int i;
		for (i = 0; i <= len; i++)
		{
			bool atEnd = i == len;
			string one = "";
			if (!atEnd)
				one = csv.Substring(i, 1);
			if (atEnd || one == ",")
			{
				string token = TrimWhitespace(csv.Substring(start, i - start));
				if (token != "")
					outNames.Insert(token);
				start = i + 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool StringInList(string name, notnull array<string> list)
	{
		if (!name || name == "")
			return false;
		int i;
		for (i = 0; i < list.Count(); i++)
		{
			if (list[i] == name)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Collects unique compartment names from all compartment managers on the vehicle hierarchy.
	//! HUD laser components call this once per instance to validate configured slot names against the actual vehicle.
	static void CollectCompartmentUniqueNames(IEntity vehicle, notnull array<string> outNames)
	{
		outNames.Clear();
		if (!vehicle)
			return;
		CollectCompartmentUniqueNamesRecursive(vehicle, outNames);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectCompartmentUniqueNamesRecursive(IEntity ent, notnull array<string> outNames)
	{
		if (!ent)
			return;
		BaseCompartmentManagerComponent mgr = BaseCompartmentManagerComponent.Cast(ent.FindComponent(BaseCompartmentManagerComponent));
		if (mgr)
			AppendSlotNamesFromManager(mgr, outNames);
		IEntity child = ent.GetChildren();
		while (child)
		{
			CollectCompartmentUniqueNamesRecursive(child, outNames);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AppendSlotNamesFromManager(BaseCompartmentManagerComponent mgr, notnull array<string> outNames)
	{
		if (!mgr)
			return;
		ref array<BaseCompartmentSlot> comps = new array<BaseCompartmentSlot>();
		mgr.GetCompartments(comps);
		int i;
		for (i = 0; i < comps.Count(); i++)
		{
			BaseCompartmentSlot slot = comps[i];
			if (!slot)
				continue;
			string id = GetSlotIdentityName(slot);
			if (id == "")
				continue;
			if (!StringInList(id, outNames))
				outNames.Insert(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPilotOrDriverSlot(BaseCompartmentSlot slot)
	{
		if (!slot)
			return false;
		return PilotCompartmentSlot.Cast(slot) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Laser code [ ] when not holding the handheld gadget: adjust vehicle marking code if in a marking-enabled seat.
	static void TryAdjustVehicleMarkingLaserCode(int delta)
	{
		Game game = GetGame();
		if (!game || !game.InPlayMode())
			return;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar))
			return;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot || !slot.GetOwner())
			return;
		IEntity markRoot = ResolveVehicleHUDMarkingRoot(slot.GetOwner());
		if (!markRoot)
			return;
		HUDLaserMarkingComponent m = HUDLaserMarkingComponent.Cast(markRoot.FindComponent(HUDLaserTurretMarkingComponent));
		if (!m)
			m = HUDLaserMarkingComponent.Cast(markRoot.FindComponent(HUDLaserCameraMarkingComponent));
		if (m)
			m.AdjustLaserCode(delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Walk parent chain for the character entity (FO vs gunner on same vehicle are different entities).
	static IEntity ResolveChimeraCharacterEntity(IEntity from)
	{
		if (!from)
			return null;
		IEntity e = from;
		for (int guard = 0; guard < 32; guard++)
		{
			if (SCR_ChimeraCharacter.Cast(e))
				return e;
			IEntity p = e.GetParent();
			if (!p || p == e)
				break;
			e = p;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! DFS: all HUD laser marking components (turret/camera subclasses; FindComponent(base) is unreliable).
	static void CollectHudLaserMarkingComponentsInHierarchy(IEntity ent, notnull array<HUDLaserMarkingComponent> outMarks)
	{
		if (!ent)
			return;
		HUDLaserMarkingComponent m = HUDLaserMarkingComponent.Cast(ent.FindComponent(HUDLaserTurretMarkingComponent));
		if (!m)
			m = HUDLaserMarkingComponent.Cast(ent.FindComponent(HUDLaserCameraMarkingComponent));
		if (m)
			outMarks.Insert(m);
		IEntity child = ent.GetChildren();
		while (child)
		{
			CollectHudLaserMarkingComponentsInHierarchy(child, outMarks);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Depth-first search for a component type (same pattern as WCS weapon modules).
	static GenericComponent FindComponentInHierarchy(IEntity root, typename componentType)
	{
		if (!root)
			return null;
		GenericComponent component = GenericComponent.Cast(root.FindComponent(componentType));
		if (component)
			return component;
		IEntity child = root.GetChildren();
		while (child)
		{
			component = FindComponentInHierarchy(child, componentType);
			if (component)
				return component;
			child = child.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! WCS vehicle weapon station for the local player's current vehicle compartment (gunner/pilot).
	static WCS_Armament_VehicleWeaponStationComponent FindVehicleWeaponStationForLocalGunner()
	{
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return null;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return null;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return null;
		IEntity slotOwner = slot.GetOwner();
		if (!slotOwner)
			return null;
		IEntity root = slotOwner.GetRootParent();
		if (!root)
			root = slotOwner;
		return WCS_Armament_VehicleWeaponStationComponent.Cast(FindComponentInHierarchy(root, WCS_Armament_VehicleWeaponStationComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! When local player enters a vehicle that has HMD_HudMarkerEligibilityVehicleComponent (IFF off, Numpad * / marking defaults).
	static void ApplyDefaultHudMarkersOnVehicleEnter(IEntity slotOwner, BaseCompartmentSlot compartment)
	{
		if (!slotOwner || !compartment)
			return;
		HUDMarkerVisibility.SetShowIffMarkers(false);
		IEntity eligRoot = ResolveVehicleHudMarkerEligibilityVehicleRoot(slotOwner);
		HMD_HudMarkerEligibilityVehicleComponent elig = HMD_HudMarkerEligibilityVehicleComponent.Cast(eligRoot.FindComponent(HMD_HudMarkerEligibilityVehicleComponent));
		if (elig)
			elig.ResetLocalVisibilityForNewOccupant();
		IEntity markRoot = ResolveVehicleHUDMarkingRoot(slotOwner);
		HUDLaserMarkingComponent mark = FindMarkingComponentOnEntity(markRoot);
		if (mark)
			mark.ForceDisableLocalMarking();
	}
}

