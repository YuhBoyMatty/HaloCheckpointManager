#pragma once
#include "imgui.h"
#include "MCCStateHook.h"
#include "ScopedCallback.h"
class GUIHeader
{
private:

	std::string headerText;
	void OnStateChange(const MCCState& newState)
	{


		headerText = std::format("Current Game: {0} ({1}), Level: {2}",
			newState.currentGameState.toString(),
			magic_enum::enum_name(newState.currentPlayState).data(),
			magic_enum::enum_name(newState.currentLevelID).data()
		);
	}

	ScopedCallback<eventpp::CallbackList<void(const MCCState&)>> MCCStateChangeCallback;

public:


	GUIHeader(std::shared_ptr<MCCStateHook> mccStateHook) :
		MCCStateChangeCallback(mccStateHook->MCCStateChangedEvent, [this](const MCCState& n) {OnStateChange(n); })
	{
		// init strings
		OnStateChange(mccStateHook->getCurrentMCCState());
		// subscribe to mccStateChange so we can update strings on change
	}

	void render() 
	{
		ImGui::Text(headerText.c_str());
	}

};