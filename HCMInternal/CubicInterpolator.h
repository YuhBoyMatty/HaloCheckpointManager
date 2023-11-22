#pragma once
#include "pch.h"
#include "IInterpolator.h"




template<typename valueType>
class CubicInterpolator : public IInterpolator<valueType>
{
private:
	float mInterpolationRate;

public:
	virtual void setInterpolationRate(float interpolationRate) override { mInterpolationRate = interpolationRate; }
	CubicInterpolator(float interpolationRate) : mInterpolationRate(interpolationRate)
	{}

	virtual void interpolate(valueType& currentValue, valueType desiredValue) override;
};

