//------------------------------------------------------------------------------------------------
//! Levels a deployed M252 mortar (world-upright, preserved yaw) when the player holds an E-Tool
//! and terrain slope at the mortar is at most 30 degrees from vertical.
//! 10s duration with the same gadget dig animation pattern as ACE trench/shovel (CMD_Item_Action, index 1).
class SCR_LevelMortarUserAction : ScriptedUserAction
{
	protected static const float LEVEL_ACTION_DURATION_S = 10.0;

	protected static const ResourceName LEVEL_FOUNDATION_PREFAB = "{6E2BB8C55AE97822}Prefabs/Structures/Military/Camps/Foundations/Foundation_LevelMortar.et";
	protected static const float LEVEL_FOUNDATION_SCALE = 0.3;
	protected static const float LEVEL_VERTICAL_OFFSET_M = 0.1;

	[Attribute(defvalue: "30", desc: "Max terrain slope angle from vertical (degrees). Action hidden if terrain is steeper.")]
	protected float m_fMaxTerrainSlopeAngleDeg = 30.0;

	[Attribute(defvalue: "1", desc: "Gadget Item_Action variant (1 = ACE trench/shovel dig)")]
	protected int m_iAnimationIndex;

	protected SCR_ChimeraCharacter m_pUserChar;
	protected SoundComponent m_pItemSoundComponent;
	protected AnimationEventID m_iSoundEventID;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_iSoundEventID = GameAnimationUtils.RegisterAnimationEvent("Sound");
		SetActionDuration(LEVEL_ACTION_DURATION_S);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		super.OnActionStart(pUserEntity);
		TryUseGadgetForDigAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.OnActionCanceled(pOwnerEntity, pUserEntity);
		CancelPlayerGadgetAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		return fProgress + timeSlice;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.PerformAction(pOwnerEntity, pUserEntity);
		CancelPlayerGadgetAnimation(pUserEntity);

		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity)
			return;

		if (!IsUserHoldingEntrenchingTool(pUserEntity))
			return;

		if (!IsTerrainSlopeAcceptable(pOwnerEntity))
			return;

		if (IsMortarAlreadyLeveled(pOwnerEntity))
			return;

		vector mat[4];
		pOwnerEntity.GetWorldTransform(mat);
		vector pos = mat[3];
		float yaw = mat[2].ToYaw();
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
		mat[3] = pos;
		mat[3][1] = mat[3][1] + LEVEL_VERTICAL_OFFSET_M;

		IEntity foundation = SpawnLevelFoundation(pOwnerEntity, mat);

		SCR_MortarLevelStateComponent levelState = SCR_MortarLevelStateComponent.Cast(pOwnerEntity.FindComponent(SCR_MortarLevelStateComponent));
		if (levelState)
			levelState.ServerCompleteLeveling(mat, foundation);
	}

	//------------------------------------------------------------------------------------------------
	//! Same gadget use path as ACE_BaseGadgetUserAction + ACE trench ETool (CMD_Item_Action).
	protected bool TryUseGadgetForDigAnimation(IEntity user)
	{
		IEntity gadget = GetHeldGadget(user);
		if (gadget)
			m_pItemSoundComponent = SoundComponent.Cast(gadget.FindComponent(SoundComponent));

		m_pUserChar = SCR_ChimeraCharacter.Cast(user);
		if (!m_pUserChar)
			return false;

		SCR_CharacterControllerComponent userCharController = SCR_CharacterControllerComponent.Cast(m_pUserChar.GetCharacterController());
		if (!userCharController)
			return false;

		if (m_pItemSoundComponent)
			userCharController.GetOnAnimationEvent().Insert(HandleItemSoundEvent);

		userCharController.m_OnItemUseEndedInvoker.Insert(OnGadgetUseEnded);
		userCharController.TryUseItemOverrideParams(GetGadgetUseParams(m_pUserChar));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected ItemUseParameters GetGadgetUseParams(IEntity user)
	{
		SCR_ChimeraCharacter userChar = SCR_ChimeraCharacter.Cast(user);
		if (!userChar)
			return null;

		CharacterAnimationComponent animationComponent = userChar.GetAnimationComponent();
		if (!animationComponent)
			return null;

		ItemUseParameters params = new ItemUseParameters();
		params.SetEntity(GetHeldGadget(user));
		params.SetAllowMovementDuringAction(false);
		params.SetKeepInHandAfterSuccess(true);
		params.SetCommandID(animationComponent.BindCommand("CMD_Item_Action"));
		params.SetCommandIntArg(GetAnimationIndex());
		return params;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetHeldGadget(notnull IEntity ent)
	{
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(ent);
		if (!gadgetManager)
			return null;

		return gadgetManager.GetHeldGadget();
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleItemSoundEvent(AnimationEventID animEventType, AnimationEventID animUserString, int intParam, float timeFromStart, float timeToEnd)
	{
		if (m_iSoundEventID != animEventType)
			return;

		if (m_pItemSoundComponent)
			m_pItemSoundComponent.SoundEvent(GameAnimationUtils.GetEventString(animUserString));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGadgetUseEnded(IEntity item, bool successful, ItemUseParameters animParams)
	{
		if (!m_pUserChar)
			return;

		SCR_CharacterControllerComponent userCharController = SCR_CharacterControllerComponent.Cast(m_pUserChar.GetCharacterController());
		if (!userCharController)
			return;

		if (m_pItemSoundComponent)
			userCharController.GetOnAnimationEvent().Remove(HandleItemSoundEvent);

		userCharController.m_OnItemUseEndedInvoker.Remove(OnGadgetUseEnded);
	}

	//------------------------------------------------------------------------------------------------
	protected void CancelPlayerGadgetAnimation(IEntity user)
	{
		if (!user)
			return;

		ChimeraCharacter userChar = ChimeraCharacter.Cast(user);
		if (!userChar)
			return;

		SCR_CharacterControllerComponent userCharController = SCR_CharacterControllerComponent.Cast(userChar.GetCharacterController());
		if (!userCharController)
			return;

		CharacterAnimationComponent userAnimationComponent = userCharController.GetAnimationComponent();
		CharacterCommandHandlerComponent cmdHandler = userAnimationComponent.GetCommandHandler();
		if (cmdHandler)
			cmdHandler.FinishItemUse(true);

		if (m_pItemSoundComponent)
			userCharController.GetOnAnimationEvent().Remove(HandleItemSoundEvent);

		userCharController.m_OnItemUseEndedInvoker.Remove(OnGadgetUseEnded);
	}

	//------------------------------------------------------------------------------------------------
	void ~SCR_LevelMortarUserAction()
	{
		CancelPlayerGadgetAnimation(m_pUserChar);
	}

	//------------------------------------------------------------------------------------------------
	int GetAnimationIndex()
	{
		return m_iAnimationIndex;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUserHoldingEntrenchingTool(IEntity user)
	{
		if (!user)
			return false;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(user);
		if (!gadgetManager)
			return false;

		IEntity gadget = gadgetManager.GetHeldGadget();
		if (!gadget)
			return false;

		if (gadget.FindComponent(SCR_CampaignBuildingGadgetToolComponent))
			return true;

		EntityPrefabData prefabData = gadget.GetPrefabData();
		if (!prefabData)
			return false;

		string prefabName = prefabData.GetPrefabName();
		if (prefabName.IndexOf("ETool") >= 0)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsTerrainSlopeAcceptable(IEntity owner)
	{
		if (!owner)
			return false;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return false;

		vector pos = owner.GetOrigin();
		vector normal = SCR_TerrainHelper.GetTerrainNormal(pos, world);
		vector up = Vector(0, 1, 0);

		float dot = vector.Dot(normal, up);
		float angleDeg = Math.Acos(Math.Clamp(dot, -1.0, 1.0)) * Math.RAD2DEG;

		return angleDeg <= m_fMaxTerrainSlopeAngleDeg;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		if (!IsUserHoldingEntrenchingTool(user))
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (IsMortarAlreadyLeveled(owner))
			return false;

		return IsTerrainSlopeAcceptable(owner);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (!IsUserHoldingEntrenchingTool(user))
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (IsMortarAlreadyLeveled(owner))
			return false;

		return IsTerrainSlopeAcceptable(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsMortarAlreadyLeveled(IEntity mortar)
	{
		SCR_MortarLevelStateComponent state = SCR_MortarLevelStateComponent.Cast(mortar.FindComponent(SCR_MortarLevelStateComponent));
		if (!state)
			return false;

		return state.IsMortarLeveled();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnLevelFoundation(IEntity mortar, vector worldMat[4])
	{
		if (!mortar)
			return null;

		Resource res = Resource.Load(LEVEL_FOUNDATION_PREFAB);
		if (!res || !res.IsValid())
			return null;

		BaseWorld world = mortar.GetWorld();
		if (!world)
			return null;

		vector foundationMat[4];
		foundationMat[0] = worldMat[0];
		foundationMat[1] = worldMat[1];
		foundationMat[2] = worldMat[2];
		foundationMat[3] = worldMat[3];

		float s = LEVEL_FOUNDATION_SCALE;
		foundationMat[0] = foundationMat[0] * s;
		foundationMat[1] = foundationMat[1] * s;
		foundationMat[2] = foundationMat[2] * s;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = foundationMat;

		IEntity foundation = GetGame().SpawnEntityPrefab(res, world, params);
		if (foundation)
			foundation.Update();

		return foundation;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Level mortar";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return true;
	}
}
