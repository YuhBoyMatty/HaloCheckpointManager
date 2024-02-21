#include "pch.h"
#include "Renderer3DImpl.h"
#include "directxtk\SimpleMath.h"



template<GameState::Value mGame>
SimpleMath::Vector3 Renderer3DImpl<mGame>::worldPointToScreenPosition(SimpleMath::Vector3 world)
{
	LOG_ONCE(PLOG_VERBOSE << "world to screen");
	auto worldPos = SimpleMath::Vector4::Transform({ world.x, world.y, world.z, 1.f }, 
		SimpleMath::Matrix::CreateWorld(
		SimpleMath::Vector3::Zero,
		SimpleMath::Vector3::Forward,
		SimpleMath::Vector3::Up
		));

	auto clipSpace = SimpleMath::Vector4::Transform(worldPos, (this->viewMatrix * this->projectionMatrix));

	// result above is actually in homogenous space, divide all components by w to get clipspace.

	clipSpace = clipSpace / clipSpace.w;

	// convert from clipSpace (-1..+1) to screenSpace (0..Width, 0..Height)
	auto out = SimpleMath::Vector3
	(
		(1.f + clipSpace.x) * 0.5 * this->screenSize.x,
		(1.f - clipSpace.y) * 0.5 * this->screenSize.y,
		clipSpace.z
	);



#ifdef HCM_DEBUG
	if (GetKeyState('9') & 0x8000)
	{
		PLOG_VERBOSE << "world to screen outputting: " << out;
	}
#endif

	LOG_ONCE_CAPTURE(PLOG_VERBOSE << "world to screen outputting: " << ss, ss = out);
	return out;
}

template<GameState::Value mGame>
float Renderer3DImpl<mGame>::cameraDistanceToWorldPoint(SimpleMath::Vector3 worldPointPosition)
{
	return SimpleMath::Vector3::Distance(this->cameraPosition, worldPointPosition);
}

template<GameState::Value mGame>
void Renderer3DImpl<mGame>::updateCameraData(const SimpleMath::Vector2& screensize)
{
	try
	{
		lockOrThrow(getGameCameraDataWeak, getGameCameraData);
		auto cameraData = getGameCameraData->getGameCameraData();
		this->cameraPosition = *cameraData.position;
		float aspectRatio = screensize.x / screensize.y; // aspect ratio is width div height!
		float verticalFov = *cameraData.FOV * (screensize.y / screensize.x); // convert camera horizontal fov to vertical. height div width!

		this->projectionMatrix = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(verticalFov, aspectRatio, 0.001f, FAR_CLIP_3D);
		auto lookAt = this->cameraPosition + *cameraData.lookDirForward;
		this->viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(this->cameraPosition, lookAt, *cameraData.lookDirUp);
		this->screenSize = screensize;
	}
	catch (HCMRuntimeException ex)
	{
		runtimeExceptions->handleMessage(ex);
	}
}



 template<GameState::Value mGame>
 Renderer3DImpl<mGame>::Renderer3DImpl(GameState game, IDIContainer& dicon)
	 : settingsWeak(dicon.Resolve<SettingsStateAndEvents>()),
	 mccStateHookWeak(dicon.Resolve<IMCCStateHook>()),
	 messagesGUIWeak(dicon.Resolve<IMessagesGUI>()),
	 runtimeExceptions(dicon.Resolve<RuntimeExceptionHandler>()),
	 getGameCameraDataWeak(resolveDependentCheat(GetGameCameraData))
 {
	 //TODO
 }

 template<GameState::Value mGame>
 Renderer3DImpl<mGame>::~Renderer3DImpl()
 {
	 //TODO
 }



 // explicit template instantiation
 template class Renderer3DImpl<GameState::Value::Halo1>;
 template class Renderer3DImpl<GameState::Value::Halo2>;
 template class Renderer3DImpl<GameState::Value::Halo3>;
 template class Renderer3DImpl<GameState::Value::Halo3ODST>;
 template class Renderer3DImpl<GameState::Value::HaloReach>;
 template class Renderer3DImpl<GameState::Value::Halo4>;