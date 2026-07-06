#include <algorithm>
#include "commands.h"

#include <sstream>

// Do not add additional headers to this list. Register feature commands from
// their own module (e.g. camera_mods.cpp), not here.
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "logger.h"
#include "memory.h"
#include "rcp.h"
#include "string_util.h"

// This is the trimmed command framework ported from Zeal (MIT). It installs a
// detour on the client's command interpreter and dispatches any registered
// slash command (added via ChatCommands::Add) before the client sees it.
// Feature modules register their own commands; only the /rcp help command is
// registered here.

void ChatCommands::print_commands() {
  std::stringstream ss;
  ss << "rof2ClientPlus commands (v" RCP_VERSION ")" << std::endl;
  ss << "-----------------------------------------------------" << std::endl;
  // Create a sorted vector of commands w/out copying.
  std::vector<std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<const RcpCommand>>>
      sorted_references;
  for (const auto &pair : CommandFunctions) {
    sorted_references.emplace_back(std::cref(pair.first), std::cref(pair.second));
  }
  std::sort(sorted_references.begin(), sorted_references.end(),
            [](const auto &a, const auto &b) { return a.first.get() < b.first.get(); });
  for (auto &pair : sorted_references) {
    ss << pair.first.get();
    const RcpCommand &c = pair.second.get();
    if (c.aliases.size() > 0) ss << " [";
    for (auto it = c.aliases.begin(); it != c.aliases.end(); ++it) {
      auto &a = *it;
      ss << a;
      if (std::next(it) != c.aliases.end()) {
        ss << ", ";
      }
    }
    if (c.aliases.size() > 0) ss << "]";

    ss << " " << c.description;
    ss << std::endl;
  }
  Rcp::Game::print_chat(ss.str());
}

bool ChatCommands::handle_chat(std::string &str_cmd) const {
  if (!tell_callback) return false;
  auto name = tell_callback();
  if (name.empty()) return false;
  str_cmd = "/tell " + name + " " + str_cmd;
  return true;
}

void __fastcall InterpretCommand(int c, int unused, Rcp::GameStructures::Entity *player, const char *cmd) {
  RcpService *rcp = RcpService::get_instance();
  std::string str_cmd = Rcp::String::trim_and_reduce_spaces(cmd);
  if (str_cmd.length() == 0) return;

  logger::logf("[cmd] InterpretCommand fired: cmd='%s' in_game=%d nregistered=%d", str_cmd.c_str(),
               (int)Rcp::Game::is_in_game(), (int)rcp->commands_hook->CommandFunctions.size());

  // NOTE: the tell-window / always-chat-here handling was dropped for the stock
  // RoF2 foundation because it dereferences the (not-yet-ported) Windows UI
  // pointer. It returns once the UI layer is ported.

  std::vector<std::string> args = Rcp::String::split(str_cmd, " ");
  const std::string &cmd_name = args.front();
  if (!args.empty() && !cmd_name.empty() && Rcp::Game::is_in_game()) {
    bool cmd_handled = false;
    auto &command_functions = rcp->commands_hook->CommandFunctions;
    if (command_functions.count(cmd_name)) {
      logger::logf("[cmd] dispatching '%s'", cmd_name.c_str());
      cmd_handled = command_functions[args[0]].callback(args);
    } else {
      // Attempt to find and call the function via an alias.
      for (const auto &command_pair : command_functions) {
        const auto &command = command_pair.second;
        if (command.callback && !command.aliases.empty()) {
          for (const auto alias : command.aliases) {
            if (!alias.empty() && alias == cmd_name) {
              cmd_handled = command.callback(args);
              break;
            }
          }
        }
      }
    }
    if (cmd_handled) {
      return;
    }
  }
  rcp->hooks->hook_map["commands"]->original(InterpretCommand)(c, unused, player, cmd);
}

void ChatCommands::Add(std::string cmd, std::vector<std::string> aliases, std::string description,
                       std::function<bool(std::vector<std::string> &args)> callback) {
  CommandFunctions[cmd] = RcpCommand(aliases, description, callback);
}

ChatCommands::~ChatCommands() {}

// Call interpret command without hitting the detour, useful for aliasing default commands.
void ForwardCommand(std::string cmd) {
  RcpService::get_instance()->hooks->hook_map["commands"]->original(InterpretCommand)(
      (int)Rcp::Game::get_game(), 0, Rcp::Game::get_self(), cmd.c_str());
}

ChatCommands::ChatCommands(RcpService *rcp) {
  Add("/rcp", {}, "Lists rof2ClientPlus commands and version.", [this](std::vector<std::string> &args) {
    print_commands();
    return true;
  });
  // Feature commands self-register from their own module (e.g. /rcpcam from MouseMods).
  rcp->hooks->Add("commands", Rcp::Game::GameInternal::fn_interpretcmd, InterpretCommand, hook_type_detour);
  logger::logf("[cmd] interpret-command hook installed at 0x%x", Rcp::Game::GameInternal::fn_interpretcmd);
}
