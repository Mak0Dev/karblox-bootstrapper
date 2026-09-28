#pragma once

#include <functional>
#include <memory>

class ShutdownRobloxApp : public std::enable_shared_from_this<ShutdownRobloxApp>
{
	int timeoutInSeconds;
	typedef std::function<bool(int, int)> Callback;
	Callback callback;
	HINSTANCE hInstance;
	std::function<HWND()> parent;
	bool result;
	std::wstring appExeName;
public:
	ShutdownRobloxApp(HINSTANCE hInstance, std::wstring name, std::function<HWND()> parent, int timeoutInSeconds, Callback callback):hInstance(hInstance), appExeName(name), parent(parent),timeoutInSeconds(timeoutInSeconds),callback(callback) {}
	~ShutdownRobloxApp(void);
	bool run();
private:
	void terminateApp(DWORD pid);
};
