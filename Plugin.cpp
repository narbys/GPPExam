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
	info.BotName = "Spain the Coward";
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
	Elite::Vector2 circMid{ agentInfo.Position + agentInfo.LinearVelocity.GetNormalized() * offset };
	float angle = angleChange * (Elite::randomFloat(2.f) - 1.f);
	m_WanderAngle += angle;
	float destx = radius * cos(m_WanderAngle);
	float desty = radius * sin(m_WanderAngle);
	Elite::Vector2 dest{ destx,desty };
	m_pInterface->Draw_Circle(circMid, radius, { 1,0,1 });
	if (Elite::DistanceSquared(agentInfo.Position, { 0,0 }) > Elite::Square(m_WorldRadius))
		m_Target = { 0,0 };
	else
		m_Target = dest + circMid;
	
	m_Steering.AutoOrient = false;
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
		if(vZombies.size() > 0 && std::any_of(vZombies.begin(), vZombies.end(), [agentInfo, houseFleeRadius](const EntityInfo& z)->bool {
			return (Elite::DistanceSquared(agentInfo.Position, z.Location) < Elite::Square(houseFleeRadius));
			}))
			m_IsNavFleeActivated = true;

		if (!agentInfo.IsInHouse || m_IsNavFleeActivated==true)
		{
			NavFlee(m_FleePoint);
		}
		else if(m_IsNavFleeActivated==false)
			Flee(m_FleePoint);
		m_CanRun = true;
		//m_CurrentHouseInfo = {}; //Remove the current house
		m_Steering.AutoOrient = false;
		return Elite::BehaviorState::Success;
	}
	else if (vZombies.size()<=0)
	{
		m_CanRun = false;
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

	//if (!agentInfo.IsInHouse ||
	//	(vZombies.size() > 0 && std::any_of(vZombies.begin(), vZombies.end(), [agentInfo, houseFleeRadius](auto z)->bool {
	//		return Elite::Distance(z.Location, agentInfo.Position) > Elite::Square(houseFleeRadius);
	//		})))
	//{
	//	m_IsFleeing = true;
	//	NavFlee(m_FleePoint);
	//}
	//else
	//	Flee(m_FleePoint);

	NavFlee(m_FleePoint);
	//std::cout << m_IsFleeing<<std::endl;
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
		//std::cout << m_IsFleeing<<std::endl;
	}
}
Elite::BehaviorState Plugin::GrabItems()	//No priority implemented yet
{
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
		for (UINT i{}; i < invCap; i++)
		{
			//m_pInterface->Item_Grab(vItems[0], currentItem);
			m_pInterface->Item_Grab(vItems[0], currentItem);
			const bool isInvSlotEmpty = m_pInterface->Inventory_AddItem(i, currentItem);
			if (isInvSlotEmpty)
				break;
		}
	}

	return Elite::BehaviorState::Success;
}
Elite::BehaviorState Plugin::EatFood()
{
	//Search inventory for food and eat it if Energy below certain value
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float useValue = 5.f;
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
			m_NeedToGetMeSomeItems = false;
			return Elite::BehaviorState::Success;
		}
		else
		{
			std::cout << "I have no food and I am now very hungry :( \n";
			m_NeedToGetMeSomeItems = true;
		}
	}

	return Elite::BehaviorState::Failure;
}
Elite::BehaviorState Plugin::UseMedkit()
{
	//Search inventory for food and eat it if Energy below certain value
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float useValue = 6.f;
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
			m_NeedToGetMeSomeItems = false;
			return Elite::BehaviorState::Success;
		}
		else
		{
			std::cout << "I have no medkit and I am very hurt :( \n";
			m_NeedToGetMeSomeItems = true;
		}
	}

	return Elite::BehaviorState::Failure;
}
Elite::BehaviorState Plugin::GoOutOfTheFuckinHouseMate()
{
	//const auto agentInfo = m_pInterface->Agent_GetInfo();
	//HouseCoords currentHouseCoords{ m_CurrentHouseInfo.Center, m_CurrentHouseInfo.Size };
	//m_VisitedHouses.push_back(currentHouseCoords);
	NavFlee(m_CurrentHouseInfo.Center);
	return Elite::BehaviorState::Success;
}
void Plugin::DiscardEmptyItems()
{
	const UINT invCap=m_pInterface->Inventory_GetCapacity();
	ItemInfo currentItem{};
	for (UINT i{}; i < invCap; i++)
	{
		m_pInterface->Inventory_GetItem(i, currentItem);
		switch (currentItem.Type)
		{
		case eItemType::PISTOL:
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
		//case eItemType::RANDOM_DROP:
		//	break;
		//case eItemType::RANDOM_DROP_WITH_CHANCE:
		//	break;
		default:
			break;
		}
	}
}
//Called only once
void Plugin::DllInit()
{

	//Called when the plugin is loaded
	//auto vHousesInFOV = GetHousesInFOV();
	m_pTree = new Elite::BehaviorTree(nullptr,
		new Elite::BehaviorSelector(
			{

				//Use items
				new Elite::BehaviorAction(std::bind(&Plugin::UseMedkit, this)),
				new Elite::BehaviorAction(std::bind(&Plugin::EatFood, this)),
				
				//Grab items in range (place below zombies later, is here for debug)
				new Elite::BehaviorAction(std::bind(&Plugin::GrabItems, this)),
				//Flee from the zombies in vision
				new Elite::BehaviorAction(std::bind(&Plugin::FleeFromZombies, this)),

				//leave house to look for items
				new Elite::BehaviorSequence({
						new Elite::BehaviorConditional([this](Elite::Blackboard* b)->bool {
								return m_pInterface->Agent_GetInfo().IsInHouse && m_NeedToGetMeSomeItems;
							}),
						new Elite::BehaviorAction(std::bind(&Plugin::GoOutOfTheFuckinHouseMate, this))
				}),

				//Go inside Current House if it exists
				new Elite::BehaviorSequence(
				{
						new Elite::BehaviorConditional([this](Elite::Blackboard* b)->bool {
						//std::cout << m_CurrentHouseInfo.Center.x << '\n';
						return(!(Elite::AreEqual(m_CurrentHouseInfo.Center.x,0.f) && Elite::AreEqual(m_CurrentHouseInfo.Center.y, 0.f)));
					}),
						new Elite::BehaviorAction([this](Elite::Blackboard* b) {
						auto agentInfo = m_pInterface->Agent_GetInfo();
						const float leaveDistance{20};
						m_Steering.AutoOrient = true;
						m_Target = m_pInterface->NavMesh_GetClosestPathPoint(m_CurrentHouseInfo.Center);
						m_pInterface->Draw_Circle(m_Target, leaveDistance, { 1,1,0 });
						if (Elite::Distance(m_Target, agentInfo.Position) > leaveDistance)
						{
							m_CurrentHouseInfo = {};
							return Elite::BehaviorState::Failure;
						}
						Seek(m_Target);
						//std::cout << "Going to house\n";
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
	//auto steering = SteeringPlugin_Output();

	//Use the Interface (IAssignmentInterface) to 'interface' with the AI_Framework
	auto agentInfo = m_pInterface->Agent_GetInfo();

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

	////Simple Seek Behaviour (towards Target)
	//m_Steering.LinearVelocity = nextTargetPos - agentInfo.Position; //Desired Velocity
	//m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	//m_Steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed
	//Seek(nextTargetPos);

	//if (Distance(nextTargetPos, agentInfo.Position) < 2.f)
	//{
	//	m_Steering.LinearVelocity = Elite::ZeroVector2;
	//}

	m_AngSpeed = agentInfo.MaxAngularSpeed;
	m_Steering.AngularVelocity = m_AngSpeed; //Rotate your character to inspect the world while walking
	//m_Steering.AutoOrient = false; //Setting AutoOrientate to TRue overrides the AngularVelocity

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
