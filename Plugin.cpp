#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include <algorithm>
#include <numeric>
//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "Spain the Progamer";
	info.Student_FirstName = "Sybran";
	info.Student_LastName = "Aerts";
	info.Student_Class = "2DAE1";
}

void Plugin::Seek(const Elite::Vector2& target)
{
	//Simple Seek Behaviour (towards Target)
	auto agentInfo = m_pInterface->Agent_GetInfo();
	m_Steering.LinearVelocity = target - agentInfo.Position; //Desired Velocity
	m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	m_Steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed
	//m_Steering.AutoOrient = false;
}

void Plugin::Flee(const Elite::Vector2& target)
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	m_Steering.LinearVelocity = agentInfo.Position - target; //Desired Velocity
	m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	m_Steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed
}

void Plugin::NavFlee(const Elite::Vector2& target)
{	
	auto agentInfo = m_pInterface->Agent_GetInfo();
	m_Steering.LinearVelocity = agentInfo.Position -  target; //Desired Velocity
	m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	m_Steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed
	const Elite::Vector2 fleeTowardsTarget = agentInfo.Position+(m_Steering.LinearVelocity * m_Steering.LinearVelocity.Magnitude());
	m_pInterface->Draw_Direction(agentInfo.Position, m_Steering.LinearVelocity, m_Steering.LinearVelocity.Magnitude(), {0,0,1});
	const Elite::Vector2 t = m_pInterface->NavMesh_GetClosestPathPoint(fleeTowardsTarget);
	Seek(t);
}

Elite::BehaviorState Plugin::Wander()
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float radius = 5.f;
	const float offset = 20.f;
	constexpr float angleChange = Elite::ToRadians(45);

	const float nearDistance{ 2.5f };
	Elite::Vector2 circMid{ agentInfo.Position + agentInfo.LinearVelocity.GetNormalized() * offset };
		float angle = angleChange * (Elite::randomFloat(2.f) - 1.f);
		m_WanderAngle += angle;
		float destx = radius * cos(m_WanderAngle);
		float desty = radius * sin(m_WanderAngle);
		Elite::Vector2 dest{ destx,desty };
		
		if (Elite::DistanceSquared(agentInfo.Position, { 0,0 }) > Elite::Square(m_WorldRadius))
			m_Target = { 0,0 };
		else
			m_Target = dest + circMid;
	m_Steering.AutoOrient = false;
	m_pInterface->Draw_Circle(circMid, radius, { 1,0,1 });
	auto nextTarget = m_pInterface->NavMesh_GetClosestPathPoint(m_Target);
	Seek(nextTarget);
	return Elite::BehaviorState::Success;
}
Elite::BehaviorState Plugin::FleeFromZombies()
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float houseFleeRadius{10};

	auto vThingsInFOV = GetEntitiesInFOV();
	std::vector<EntityInfo> vZombies(vThingsInFOV.size());
	auto it = std::copy_if(vThingsInFOV.begin(), vThingsInFOV.end(), vZombies.begin(), [](auto a)->bool {return a.Type == eEntityType::ENEMY; });
	vZombies.resize(std::distance(vZombies.begin(), it));

	if (m_IsFleeing == true)
	{

		if (vZombies.size() > 0)
		{
			Elite::Vector2 avPos{};
			for (const auto& zombie : vZombies)
			{
				avPos += zombie.Location;
			}
			avPos /= float(vZombies.size());
			m_FleePoint = avPos;

			if (std::any_of(vZombies.begin(), vZombies.end(), [agentInfo, houseFleeRadius](const EntityInfo& z)->bool {
				return (Elite::DistanceSquared(agentInfo.Position, z.Location) < Elite::Square(houseFleeRadius));
				}))
				m_IsNavFleeActivated = true;
		}

		if (!agentInfo.IsInHouse || m_IsNavFleeActivated==true)
		{
			NavFlee(m_FleePoint);
		}
		else if(m_IsNavFleeActivated==false)
			Flee(m_FleePoint);

		m_Steering.AutoOrient = false;
		return Elite::BehaviorState::Success;
	}
	else if (vZombies.empty())
	{
		return Elite::BehaviorState::Failure;
	}

	//CALCULATE AVERAGE ZOMBIE 
	Elite::Vector2 avPos{};
	for (const auto& zombie : vZombies)
	{
		avPos += zombie.Location;
	}
	avPos /= float(vZombies.size());
	m_FleePoint = avPos;
	m_IsFleeing = true;
	m_FleeTime = vZombies.size()* 3.f; //set timer for fleeing to X seconds

	NavFlee(m_FleePoint);

	return Elite::BehaviorState::Success;
}
void Plugin::UpdateTimers(float dt)
{
	if (m_FleeTime > 0)
		m_FleeTime -= dt;
	else
	{
		m_IsFleeing = false;
		m_IsNavFleeActivated = false;
		m_FleeTime = 0;
		m_CanRun = false;
	}

	if (m_StayInHouseTimer > 0)
		m_StayInHouseTimer -= dt;
	else if (m_pInterface->Agent_GetInfo().IsInHouse && m_IsHouseTimerSet)
	{
		m_IsHouseTimerSet = false;
		m_NeedToGetOutOfHouse = true;
	}

	if (m_ClearHouseListTimer > 0)
		m_ClearHouseListTimer -= dt;
	else
	{
		m_EmptyHousesCoords.clear();
		m_ClearHouseListTimer = 100.f;
	}

	if (m_SprintTimer > 0)
		m_SprintTimer -= dt;
	else
		m_CanRun = false;
}
Elite::BehaviorState Plugin::GrabItems()	//No priority implemented yet
{
	if (m_ItemLookingFor == eItemType::_LAST && InventoryCount() == 5)
		return Elite::BehaviorState::Failure;
	auto agentInfo = m_pInterface->Agent_GetInfo();

	auto vThingsInFOV = GetEntitiesInFOV();
	if (vThingsInFOV.empty())
		return Elite::BehaviorState::Failure;
	std::vector<EntityInfo> vItems(vThingsInFOV.size());
	auto it = std::copy_if(vThingsInFOV.begin(), vThingsInFOV.end(), vItems.begin(), [](auto a)->bool {return a.Type == eEntityType::ITEM; });
	vItems.resize(std::distance(vItems.begin(), it));
	if (vItems.empty())
		return Elite::BehaviorState::Failure;
	ItemInfo currentItem;
	m_pInterface->Item_GetInfo(vItems[0], currentItem);
	Seek(currentItem.Location);
	if (Elite::DistanceSquared(currentItem.Location, agentInfo.Position) < agentInfo.GrabRange)
	{
		const UINT invCap = m_pInterface->Inventory_GetCapacity();
		UINT currentSlot{};
		bool isInvSlotEmpty{};
		for (UINT i{}; i < invCap; i++)
		{
			//m_pInterface->Item_Grab(vItems[0], currentItem);
			m_pInterface->Item_Grab(vItems[0], currentItem);
			isInvSlotEmpty = m_pInterface->Inventory_AddItem(i, currentItem);
			if (isInvSlotEmpty)
				break;
		}
		if (!isInvSlotEmpty)
				return Elite::BehaviorState::Failure;

		if (currentItem.Type == m_ItemLookingFor )
		{
			m_StayInHouseTimer = 0;
			m_IsHouseTimerSet = false;
			m_ItemLookingFor = eItemType::_LAST;
		}
		else if(m_ItemLookingFor != eItemType::_LAST)
		{
			m_StayInHouseTimer = 3.5f;
			m_IsHouseTimerSet = true;
		}
	}

	return Elite::BehaviorState::Success;
}
Elite::BehaviorState Plugin::EatFood()
{
	//Search inventory for food and eat it if Energy below certain value
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float useValue = 7.f;
	if (agentInfo.Energy <= useValue)
	{
		ItemInfo currentItem;
		const UINT invCap=m_pInterface->Inventory_GetCapacity();
		bool itemFound{};
		UINT itemSlot{};
		for (UINT i{}; i < invCap; i++)
		{
			m_pInterface->Inventory_GetItem(i, currentItem);
			if (currentItem.Type == eItemType::FOOD)
			{
				itemFound = true;
				itemSlot = i;
				break;
			}
		}
		if (itemFound)
		{
			m_pInterface->Inventory_UseItem(itemSlot);
			return Elite::BehaviorState::Success;
		}
		else if(agentInfo.Energy+0.2f > useValue && agentInfo.IsInHouse)
		{
			std::cout << "I have no food and I am now very hungry :( \n";
			m_StayInHouseTimer = 3.f;
			m_IsHouseTimerSet = true;
			m_ItemLookingFor = eItemType::FOOD;
		}
	}

	return Elite::BehaviorState::Failure;
}
Elite::BehaviorState Plugin::UseMedkit()
{
	//Search inventory for food and eat it if Energy below certain value
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float useValue = 7.f;
	if (agentInfo.Health <= useValue)
	{
		ItemInfo currentItem;
		const UINT invCap = m_pInterface->Inventory_GetCapacity();
		bool itemFound{};
		UINT itemSlot{};
		for (UINT i{}; i < invCap; i++)
		{
			m_pInterface->Inventory_GetItem(i, currentItem);
			if (currentItem.Type == eItemType::MEDKIT)
			{
				itemFound = true;
				itemSlot = i;
				break;
			}
		}
		if (itemFound)
		{
			m_pInterface->Inventory_UseItem(itemSlot);
			m_IsHouseTimerSet = false;
			return Elite::BehaviorState::Success;
		}
		else if(agentInfo.Health + 0.2f > useValue && agentInfo.IsInHouse)
		{
			std::cout << "I have no medkit and I am very hurt :( \n";
			m_StayInHouseTimer = 3.f;
			m_IsHouseTimerSet = true;
			m_ItemLookingFor = eItemType::MEDKIT;
		}
	}

	return Elite::BehaviorState::Failure;
}
Elite::BehaviorState Plugin::GoOutOfHouse()
{
	if (!m_NeedToGetOutOfHouse)
		return Elite::BehaviorState::Failure;
	else if (!m_pInterface->Agent_GetInfo().IsInHouse)
	{
		m_NeedToGetOutOfHouse = false;
		return Elite::BehaviorState::Failure;
	}
	auto it = std::find(m_EmptyHousesCoords.begin(), m_EmptyHousesCoords.end(), m_CurrentHouseInfo.Center);
	if (it == m_EmptyHousesCoords.end())
	{
		m_EmptyHousesCoords.push_back(m_CurrentHouseInfo.Center);
	}
	NavFlee(m_CurrentHouseInfo.Center);
	return Elite::BehaviorState::Success;
}
void Plugin::DiscardEmptyItems()
{
	const UINT invCap=m_pInterface->Inventory_GetCapacity();
	ItemInfo currentItem{};
	currentItem.Type = eItemType::_LAST; //0 is PISTOL so put it to _LAST to avoid errors
	for (UINT i{}; i < invCap; i++)
	{
		m_pInterface->Inventory_GetItem(i, currentItem);
		switch (currentItem.Type)
		{
		case eItemType::PISTOL:
		{
			const int a = m_pInterface->Weapon_GetAmmo(currentItem);
			if (a <= 0)
				m_pInterface->Inventory_RemoveItem(i);
		}
			break;
		case eItemType::MEDKIT:
		{
			const int h = m_pInterface->Medkit_GetHealth(currentItem);
			if (h <= 0)
				m_pInterface->Inventory_RemoveItem(i);
		}
			break;
		case eItemType::FOOD:
		{
			const int e = m_pInterface->Food_GetEnergy(currentItem);
			if (e <= 0)
				m_pInterface->Inventory_RemoveItem(i);
		}
			break;
		case eItemType::GARBAGE:
			m_pInterface->Inventory_RemoveItem(i);
			break;
		default:
			break;
		}
	}
}
void Plugin::Shoot()
{
	auto agentInfo = m_pInterface->Agent_GetInfo();

	auto vThingsInFOV = GetEntitiesInFOV();
	std::vector<EntityInfo> vZombies(vThingsInFOV.size());
	auto it = std::copy_if(vThingsInFOV.begin(), vThingsInFOV.end(), vZombies.begin(), [](auto a)->bool {return a.Type == eEntityType::ENEMY; });
	vZombies.resize(std::distance(vZombies.begin(), it));

	if (vZombies.empty())
		return;

	//Turn towards the zombie
	const Elite::Vector2 zombieTarget = vZombies[0].Location;
	const Elite::Vector2 toTarget{ zombieTarget - agentInfo.Position };

	const  float to{ atan2f(toTarget.y, toTarget.x) + float(E_PI_2) };
	float from{ agentInfo.Orientation };
	from = atan2f(sinf(from), cosf(from));
	float desired = to - from;

	const float Pi2 = float(E_PI) * 2.f;
	if (desired > E_PI)
		desired -= Pi2;
	else if (desired < -E_PI)
		desired += Pi2;

	// multiply desired by some value to make it go as fast as possible (30.f)
	m_Steering.AngularVelocity = desired * agentInfo.MaxAngularSpeed;

	m_Steering.AutoOrient = false;

	//Check if you're facing the zombie
	if (abs(desired) > 0.035f)
		return;
	else
		m_Steering.AngularVelocity = 0;

	//Search pistol
	const UINT invCap = m_pInterface->Inventory_GetCapacity();
	ItemInfo currentItem{};
	currentItem.Type = eItemType::_LAST; //0 is PISTOL so put it to _LAST to avoid errors
	for (UINT i{}; i < invCap; i++)
	{
		m_pInterface->Inventory_GetItem(i, currentItem);
		if (currentItem.Type == eItemType::PISTOL)
			if (m_pInterface->Weapon_GetAmmo(currentItem) > 0)
				m_pInterface->Inventory_UseItem(i);	//Shoot the zombie
	}
}
int Plugin::InventoryCount()
{
	UINT invCap = m_pInterface->Inventory_GetCapacity();
	int itemsInInventory{};
	ItemInfo inf{};
	for (UINT i{}; i < invCap; i++)
	{
		if (m_pInterface->Inventory_GetItem(i, inf))
			itemsInInventory++;
	}
	return itemsInInventory;
}
int Plugin::InventoryCountOfType(eItemType type)
{
	UINT invCap = m_pInterface->Inventory_GetCapacity();
	int itemsInInventory{};
	ItemInfo inf{};
	for (UINT i{}; i < invCap; i++)
	{
		if (m_pInterface->Inventory_GetItem(i, inf))
			if(inf.Type==type)
				itemsInInventory++;
	}
	return itemsInInventory;
}
Elite::BehaviorState Plugin::FleeFromPurgeZone()
{
	const auto agentInfo=m_pInterface->Agent_GetInfo();
	const auto v = GetEntitiesInFOV();
	const auto it = std::find_if(v.begin(), v.end(), [](const EntityInfo& e) {return e.Type == eEntityType::PURGEZONE; });
	if (it == v.end())
		return Elite::BehaviorState::Failure;

	const EntityInfo& entInf = *it;
	PurgeZoneInfo purgeInf{};

	m_pInterface->PurgeZone_GetInfo(entInf, purgeInf);
	if (Elite::DistanceSquared(purgeInf.Center, agentInfo.Position) >= Elite::Square(purgeInf.Radius))
	{
		m_Target = purgeInf.Center;
		NavFlee(purgeInf.Center);
		m_CanRun = true;
		return Elite::BehaviorState::Success;
	}
	else
		return Elite::BehaviorState::Failure;
}
//Called only once
void Plugin::DllInit()
{

	//Called when the plugin is loaded
	m_pTree = new Elite::BehaviorTree(nullptr,
		new Elite::BehaviorSelector(
			{
				//Run from purgezones
				new Elite::BehaviorAction(std::bind(&Plugin::FleeFromPurgeZone,this)),

				//Use items
				new Elite::BehaviorAction(std::bind(&Plugin::UseMedkit, this)),
				new Elite::BehaviorAction(std::bind(&Plugin::EatFood, this)),

				//Grab items in range
				new Elite::BehaviorAction(std::bind(&Plugin::GrabItems, this)),
				//Flee from the zombies in vision
				new Elite::BehaviorAction(std::bind(&Plugin::FleeFromZombies, this)),
				//leave house to look for items
				new Elite::BehaviorAction(std::bind(&Plugin::GoOutOfHouse, this)),
				//Go inside Current House if it exists
				new Elite::BehaviorSequence(
				{
								new Elite::BehaviorConditional([this](Elite::Blackboard* b)->bool {
								return(!(Elite::AreEqual(m_CurrentHouseInfo.Center.x,0.f) && Elite::AreEqual(m_CurrentHouseInfo.Center.y, 0.f))
									&& std::find(m_EmptyHousesCoords.begin(),m_EmptyHousesCoords.end(), m_CurrentHouseInfo.Center)==m_EmptyHousesCoords.end());
							}),
								new Elite::BehaviorAction([this](Elite::Blackboard* b) {
								auto agentInfo = m_pInterface->Agent_GetInfo();
								float leaveDistance{20};
								m_Steering.AutoOrient = true;
								m_Target = m_pInterface->NavMesh_GetClosestPathPoint(m_CurrentHouseInfo.Center);
								m_pInterface->Draw_Circle(m_Target, leaveDistance, { 1,1,0 });
								if (Elite::Distance(m_Target, agentInfo.Position) > leaveDistance)
								{
									m_CurrentHouseInfo = {};
									return Elite::BehaviorState::Failure;
								}
								Seek(m_Target);
								if (agentInfo.IsInHouse && m_IsHouseTimerSet == false)
								{
									//get out after a while
									m_StayInHouseTimer = 10.f;
									m_IsHouseTimerSet = true;
								}
								return Elite::BehaviorState::Success;
							})
						}),
				//When see house, go in house
				new Elite::BehaviorSequence(
				{
							new Elite::BehaviorConditional([this](Elite::Blackboard* b)->bool {
									auto vHousesInFOV = GetHousesInFOV();
									return (vHousesInFOV.size() > 0);
							}),
							new Elite::BehaviorAction([this](Elite::Blackboard* b) {
									auto vHousesInFOV = GetHousesInFOV();
									//Set the current house to the house you saw
									m_CurrentHouseInfo = vHousesInFOV[0];
									if (!(std::find(m_EmptyHousesCoords.begin(), m_EmptyHousesCoords.end(),
										m_CurrentHouseInfo.Center) == m_EmptyHousesCoords.end()))
										return Elite::BehaviorState::Failure;
									m_Target = m_pInterface->NavMesh_GetClosestPathPoint(m_CurrentHouseInfo.Center);
									Seek(m_Target);
									std::cout << "Found house";
									return Elite::BehaviorState::Success;
							})
					}),


				//By default wander the world
				new Elite::BehaviorAction(std::bind(&Plugin::Wander, this))
			})
	);
}

//Called only once
void Plugin::DllShutdown()
{
	//Called wheb the plugin gets unloaded
}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be usefull to inspect certain behaviours (Default = false)
	params.AutoGrabClosestItem = true; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::Update(float dt)
{
	//Demo Event Code
	//In the end your AI should be able to walk around without external input
	if (m_pInterface->Input_IsMouseButtonUp(Elite::InputMouseButton::eLeft))
	{
		//Update target based on input
		Elite::MouseData mouseData = m_pInterface->Input_GetMouseData(Elite::InputType::eMouseButton, Elite::InputMouseButton::eLeft);
		const Elite::Vector2 pos = Elite::Vector2(static_cast<float>(mouseData.X), static_cast<float>(mouseData.Y));
		m_Target = m_pInterface->Debug_ConvertScreenToWorld(pos);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Space))
	{
		m_CanRun = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Left))
	{
		m_AngSpeed -= Elite::ToRadians(10);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Right))
	{
		m_AngSpeed += Elite::ToRadians(10);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_G))
	{
		m_GrabItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_U))
	{
		m_UseItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_R))
	{
		m_RemoveItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyUp(Elite::eScancode_Space))
	{
		m_CanRun = false;
	}
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	m_pTree->Update(dt);
	UpdateTimers(dt);
	DiscardEmptyItems();
	Shoot();

	//Use the Interface (IAssignmentInterface) to 'interface' with the AI_Framework
	auto agentInfo = m_pInterface->Agent_GetInfo();

	//set sprint to true when bitten
	if (agentInfo.Bitten)
	{
		m_CanRun = true;
		m_SprintTimer = { 2.f };
	}
	if (!agentInfo.IsInHouse&&m_IsHouseTimerSet==true)
	{
		m_IsHouseTimerSet = false;
		m_StayInHouseTimer = 0;
	}

	auto nextTargetPos = m_Target; //To start you can use the mouse position as guidance

	auto vHousesInFOV = GetHousesInFOV();//uses m_pInterface->Fov_GetHouseByIndex(...)
	auto vEntitiesInFOV = GetEntitiesInFOV(); //uses m_pInterface->Fov_GetEntityByIndex(...)

	for (auto& e : vEntitiesInFOV)
	{
		if (e.Type == eEntityType::PURGEZONE)
		{
			PurgeZoneInfo zoneInfo;
			m_pInterface->PurgeZone_GetInfo(e, zoneInfo);
			std::cout << "Purge Zone in FOV:" << e.Location.x << ", " << e.Location.y << " ---EntityHash: " << e.EntityHash << "---Radius: " << zoneInfo.Radius << std::endl;
		}
	}

	//INVENTORY USAGE DEMO
	//********************

	if (m_GrabItem)
	{
		ItemInfo item;
		//Item_Grab > When DebugParams.AutoGrabClosestItem is TRUE, the Item_Grab function returns the closest item in range
		//Keep in mind that DebugParams are only used for debugging purposes, by default this flag is FALSE
		//Otherwise, use GetEntitiesInFOV() to retrieve a vector of all entities in the FOV (EntityInfo)
		//Item_Grab gives you the ItemInfo back, based on the passed EntityHash (retrieved by GetEntitiesInFOV)
		if (m_pInterface->Item_Grab({}, item))
		{
			//Once grabbed, you can add it to a specific inventory slot
			//Slot must be empty
			m_pInterface->Inventory_AddItem(0, item);
		}
	}

	if (m_UseItem)
	{
		//Use an item (make sure there is an item at the given inventory slot)
		m_pInterface->Inventory_UseItem(0);
	}

	if (m_RemoveItem)
	{
		//Remove an item from a inventory slot
		m_pInterface->Inventory_RemoveItem(0);
	}

	m_Steering.AngularVelocity = m_AngSpeed; //Rotate your character to inspect the world while walking
	//m_Steering.AutoOrient = true; //Setting AutoOrientate to TRue overrides the AngularVelocity

	m_Steering.RunMode = m_CanRun; //If RunMode is True > MaxLinSpd is increased for a limited time (till your stamina runs out)

								 //SteeringPlugin_Output is works the exact same way a SteeringBehaviour output
								 //@End (Demo Purposes)
	m_GrabItem = false; //Reset State
	m_UseItem = false;
	m_RemoveItem = false;

	return m_Steering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	//This Render function should only contain calls to Interface->Draw_... functions
	m_pInterface->Draw_SolidCircle(m_Target, .7f, { 0,0 }, { 1, 0, 0 });
	//m_pInterface->Draw_Segment(agentInfo.Position, m_Steering.LinearVelocity, { 0,0,1 });
	m_pInterface->Draw_Circle({ 0,0 }, m_WorldRadius, { 0,1,1 });
	//m_pInterface->Draw_Circle(agentInfo.Position, 10, { 0,1,1 });
}

vector<HouseInfo> Plugin::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}
