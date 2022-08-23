#pragma once

class Listener : public INetworkEventListener
{
public:
	Listener(LanServer* server);
	~Listener() = default;

	virtual bool OnConnectionRequest(const unsigned long IP, const unsigned short port) override;
	virtual void OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port) override;
	virtual void OnClientLeave(const sessionID_t ID) override;
	virtual void OnRecv(const sessionID_t ID, const Message* message) override;
	virtual void OnError(const int errorCode, const wchar_t* message) override;
private:
	LanServer* mServer;
};