#pragma once
#include "HaloEnums.h"
#include "MessagesGUI.h"
#include "FailedServiceInfo.h"

class CheatBase
{
private:


protected:
	GameState mGame;
	virtual void initialize() = 0;

public:
	CheatBase(GameState gameImpl) : mGame(gameImpl) {}
	virtual ~CheatBase() = default;

	void initializeBase()
	{
		attemptedInitialization = true;
		try
		{
			initialize();
			successfullyInitialized = true;
		}
		catch (HCMInitException ex)
		{

			FailedServiceInfo::addServiceFailure(std::string{GameStateToString.at(mGame) + "::" + std::string{getName()}}, ex);
		}
		
	}


	virtual std::string_view getName() = 0;
	bool successfullyInitialized = false;
	bool attemptedInitialization = false;

};

template <typename T>
concept CheatBaseTemplate = std::is_base_of<CheatBase, T>::value;


