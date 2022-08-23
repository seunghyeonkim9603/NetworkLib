#pragma once

class INetworkEventListener
{
public:
	virtual bool OnConnectionRequest(const unsigned long IP, const unsigned short port) = 0;
	virtual void OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port) = 0;
	virtual void OnClientLeave(const sessionID_t ID) = 0;
	virtual void OnRecv(const sessionID_t ID, const Message* message) = 0;
	virtual void OnError(const int errorCode, const wchar_t* message) = 0;
};