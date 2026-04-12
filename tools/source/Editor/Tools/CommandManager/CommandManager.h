#pragma once

#include <memory>
#include <stack>

class AbstractCommand;

class CommandManager
{
public:
	static void DoCommand(std::shared_ptr<AbstractCommand> aCommand);
	static void Undo();
	static void Redo();

private:
	static std::stack<std::shared_ptr<AbstractCommand>> myUndoStack;
	static std::stack<std::shared_ptr<AbstractCommand>> myRedoStack;
};