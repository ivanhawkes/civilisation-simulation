#pragma once

#include <atomic>
#include <ftxui/component/screen_interactive.hpp>
#include <thread>


class CivSimTui
{
  public:
	CivSimTui();

	void Run();

  private:
	std::vector<std::string> commands;

	std::string Eval(const std::string &line, ftxui::ScreenInteractive &screen);

	// Add to your private member variables
	std::chrono::steady_clock::time_point start_time;

	// Allow us to run asynchronously for refreshing the UI and other background tasks.
	std::thread refresh_thread;

	// Signal when it is time for the background thread to stop.
	std::atomic<bool> run_thread{true};

	// Allow highlighting of the last command based on its success or failure status.
	bool last_command_failed = false;
};
