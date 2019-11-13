#pragma once

#include "../interfaces/IFlowControlUnit.h"
#include "../interfaces/IErrorVerificationUnit.h"
#include "../interfaces/IFailureInsertionUnit.h"

class IBaseService
{
public:
    IBaseService(IFlowControlUnit flowControl)
        : m_flowContrulUnit(flowControl){};
    virtual ~IBaseService() {};
    virtual void Start() = 0;

protected:
    IFlowControlUnit m_flowContrulUnit;
};