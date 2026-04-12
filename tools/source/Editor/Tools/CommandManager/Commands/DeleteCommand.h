#pragma once
#include <AbstractCommand.h>
#include <memory>
#include <vector>

class Scene;
class ModelInstance;

class DeleteCommand : public AbstractCommand
{
public:
	DeleteCommand(std::shared_ptr<Scene> aScene, std::vector<ModelInstance*> aInstances);
	~DeleteCommand();

	bool Execute() override;
	bool Undo()    override;

private:
	std::shared_ptr<Scene>       myScene;
	std::vector<ModelInstance*>  myInstances;
	bool                         myOwnsInstances = false;
};
