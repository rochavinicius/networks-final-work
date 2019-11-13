#pragma once

#include "IFailureInsertionUnit.h"

class IFlowControlUnit
{
public:
    IFlowControlUnit(IFailureInsertionUnit failureInsertionUnit,
                     IErrorVerificationUnit errorVerification){};
    virtual ~IFlowControlUnit(){};

    void SendFile(std::string &name, int &socket, struct sockaddr_in &sock_addr);

private:
    IFailureInsertionUnit m_FailureInsertionUnit;
    IErrorVerificationUnit m_ErrorVerificationUnit;
};