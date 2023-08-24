#pragma once
#include "pch.h"
#include "GameState.h"
#include "OptionsState.h"
#include "MultilevelPointer.h"
#include "PointerManager.h"
#include "MessagesGUI.h"
#include "CheatBase.h"
#include "GameStateHook.h"
class ForceRevert : public CheatBase
{
private:

	eventpp::CallbackList<void()>::Handle mForceRevertCallbackHandle = {};

	void onForceRevert()
	{
		if (GameStateHook::getCurrentGameState() != mGame) return;

		PLOG_DEBUG << "Force Revert called";
		try
		{
			byte enableFlag = 1;
			if (!forceRevertFlag.get()->writeData(&enableFlag)) throw HCMRuntimeException(std::format("Failed to write Revert flag {}", MultilevelPointer::GetLastError()));
			MessagesGUI::addMessage("Revert forced.");
		}
		catch (HCMRuntimeException ex)
		{
			RuntimeExceptionHandler::handleMessage(ex);
		}

	}

	//data
	std::shared_ptr<MultilevelPointer> forceRevertFlag;

public:

	ForceRevert(GameState gameImpl) : CheatBase(gameImpl)
	{

	}

	~ForceRevert()
	{
		// unsubscribe 
		if (mForceRevertCallbackHandle)
			OptionsState::forceRevertEvent.get()->remove(mForceRevertCallbackHandle);
	}

	void initialize() override
	{
		// exceptions thrown up to creator
		forceRevertFlag = PointerManager::getData<std::shared_ptr<MultilevelPointer>>("forceRevertFlag", mGame);

		// subscribe to forceRevert event
		mForceRevertCallbackHandle = OptionsState::forceRevertEvent.get()->append([this]() { this->onForceRevert(); });

	}

	std::string_view getName() override { return "Force Revert"; }

};