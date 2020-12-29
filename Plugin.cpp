#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include <algorithm>
//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "Spain but the a is silent";
	info.Student_FirstName = "Sybran";
	info.Student_LastName = "Aerts";
	info.Student_Class = "2DAE1";
}

Elite::BehaviorState Plugin::Wander()
{
	auto agentInfo = m_pInterface->Agent_GetInfo();
	const float radius = 5.f;
	const float offset = 10.f;
	constexpr float angleChange = Elite::ToRadians(45);
	Elite::Vector2 circMid{ agentInfo.Position + agentInfo.LinearVelocity.GetNormalized() * offset };
	float angle = angleChange * (Elite::randomFloat(2.f) - 1.f);
	m_WanderAngle += angle;
	float destx = radius * cos(m_WanderAngle);
	float desty = radius * sin(m_WanderAngle);
	Elite::Vector2 dest{ destx,desty };
	m_Target = dest + circMid;
	return Elite::BehaviorState::Success;
}
Elite::BehaviorState Plugin::FleeFromZombies()
{
	auto vThingsInFOV = GetEntitiesInFOV();
	std::vector<EntityInfo> vZombies(vThingsInFOV.size());
	auto it = std::copy_if(vThingsInFOV.begin(), vThingsInFOV.end(), vZombies.begin(), [](auto a)->bool {return a.Type == eEntityType::ENEMY; });
	vZombies.resize(std::distance(vZombies.begin(), it));
	if(vZombies.size()<=0)
		return Elite::BehaviorState::Failure;
	//flee code here, or in another function that gets called here
	std::cout << "OHNO zombies, i better run the fuck away\n";
	return Elite::BehaviorState::Success;
}
//Called only once
void Plugin::DllInit()
{

	//Called when the plugin is loaded
	//auto vHousesInFOV = GetHousesInFOV();
	m_pTree = new Elite::BehaviorTree(nullptr,
		new Elite::BehaviorSelector(
			{
				//Flee from the zombies in vision
				new Elite::BehaviorAction(std::bind(&Plugin::FleeFromZombies, this)),

				//Go inside Current House if it exists
				new Elite::BehaviorSequence(
				{
						new Elite::BehaviorConditional([this](Elite::Blackboard* b)->bool {
						//std::cout << m_CurrentHouseInfo.Center.x << '\n';
						return(!(Elite::AreEqual(m_CurrentHouseInfo.Center.x,0.f) && Elite::AreEqual(m_CurrentHouseInfo.Center.y, 0.f)));
					}),
				new Elite::BehaviorAction([this](Elite::Blackboard* b) {
						m_Target = m_pInterface->NavMesh_GetClosestPathPoint(m_CurrentHouseInfo.Center);
						//std::cout << "Going to house\n";
						return Elite::BehaviorState::Success;
					})
				}),
			//When seeing house, go in house
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
							std::cout << "Found house";
							return Elite::BehaviorState::Success;
					})
			}),
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
	auto steering = SteeringPlugin_Output();

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

	//Go hide in a house like the coward you are
	//if (vHousesInFOV.size() > 0)
	//{
	//nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(vHousesInFOV[0].Center-vHousesInFOV[0].Size/2);
	//}


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

	//Simple Seek Behaviour (towards Target)
	steering.LinearVelocity = nextTargetPos - agentInfo.Position; //Desired Velocity
	steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed

	if (Distance(nextTargetPos, agentInfo.Position) < 2.f)
	{
		steering.LinearVelocity = Elite::ZeroVector2;
	}

	//steering.AngularVelocity = m_AngSpeed; //Rotate your character to inspect the world while walking
	steering.AutoOrient = true; //Setting AutoOrientate to TRue overrides the AngularVelocity

	steering.RunMode = m_CanRun; //If RunMode is True > MaxLinSpd is increased for a limited time (till your stamina runs out)

								 //SteeringPlugin_Output is works the exact same way a SteeringBehaviour output
								 //@End (Demo Purposes)
	m_GrabItem = false; //Reset State
	m_UseItem = false;
	m_RemoveItem = false;

	return steering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	//This Render function should only contain calls to Interface->Draw_... functions
	m_pInterface->Draw_SolidCircle(m_Target, .7f, { 0,0 }, { 1, 0, 0 });
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
