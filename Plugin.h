#pragma once
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"
#include "EBehaviorTree.h"

class IBaseInterface;
class IExamInterface;

class Plugin :public IExamPlugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void Update(float dt) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

private:
	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	vector<HouseInfo> GetHousesInFOV() const;
	vector<EntityInfo> GetEntitiesInFOV() const;

	Elite::Vector2 m_Target = {};
	bool m_CanRun = false; //Demo purpose
	bool m_GrabItem = false; //Demo purpose
	bool m_UseItem = false; //Demo purpose
	bool m_RemoveItem = false; //Demo purpose
	float m_AngSpeed = 50.f; //Demo purpose

	//MyMemberVars
	Elite::BehaviorTree* m_pTree{};
	float m_WanderAngle=0.f;
	HouseInfo m_CurrentHouseInfo{};	
	SteeringPlugin_Output m_Steering;
	Elite::Vector2 m_FleePoint{};
	bool m_IsFleeing{};
	bool m_IsNavFleeActivated{};
	float m_FleeTime{};
	const float m_WorldRadius{250.f};
	bool m_NeedToGetOutOfHouse{ };
	std::vector<Elite::Vector2> m_EmptyHousesCoords;
	eItemType m_ItemLookingFor{eItemType::_LAST};
	float m_StayInHouseTimer{};
	bool m_IsHouseTimerSet{};
	float m_ClearHouseListTimer{ 100.f };
	float m_SprintTimer{};

	//MyMemberFunctions
	void Seek(const Elite::Vector2& target);
	void Flee(const Elite::Vector2& target);
	void NavFlee(const Elite::Vector2& target);
	Elite::BehaviorState Wander();
	Elite::BehaviorState FleeFromZombies();
	void UpdateTimers(float dt);
	Elite::BehaviorState GrabItems();
	Elite::BehaviorState EatFood();
	Elite::BehaviorState UseMedkit();
	Elite::BehaviorState GoOutOfHouse();
	void DiscardEmptyItems();
	void Shoot();	
	int InventoryCount();
	int InventoryCountOfType(eItemType type);
	Elite::BehaviorState FleeFromPurgeZone();

};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}