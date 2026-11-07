#pragma once

#include <ftxui/component/screen_interactive.hpp>


class CivSimTui {
public:
  CivSimTui();

  void Run();

private:
  std::vector<std::string> commands;

  std::string Eval(const std::string &line, ftxui::ScreenInteractive &screen);
};

