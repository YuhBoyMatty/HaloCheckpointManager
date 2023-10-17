#pragma once
#include "PointerManager.h"
#include "FreeMCCCursor.h"
#include "BlockGameInput.h"


class ControlServiceContainer
{
public:
	std::optional<std::shared_ptr<FreeMCCCursor>> freeMCCSCursorService = std::nullopt;
	std::optional<HCMInitException> freeMCCSCursorServiceFailure = std::nullopt;

	std::optional<std::shared_ptr<BlockGameInput>> blockGameInputService = std::nullopt;
	std::optional<HCMInitException> blockGameInputServiceFailure = std::nullopt;

	ControlServiceContainer(std::shared_ptr<PointerManager> ptr)
	{
		try
		{
			freeMCCSCursorService = std::make_optional<std::shared_ptr<FreeMCCCursor>>(std::make_shared<FreeMCCCursor>(ptr));
		}
		catch (HCMInitException ex)
		{
			PLOG_ERROR << "Failed to create freeMCCCursorService, " << ex.what();
			freeMCCSCursorServiceFailure = ex;
		}
		try
		{
			blockGameInputService = std::make_optional<std::shared_ptr<BlockGameInput>>(std::make_shared<BlockGameInput>(ptr));
		}
		catch (HCMInitException ex)
		{
			PLOG_ERROR << "Failed to create blockGameInputService, " << ex.what();
			blockGameInputServiceFailure = ex;
		}
	}



};